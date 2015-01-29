#include <thread>
#include <memory>
#include <array>
#include <functional>
#include <time.h>
#include <queue>
#include <stdlib.h>

#include <boost/asio.hpp>
#include <boost/asio/posix/basic_descriptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>

#include <statefs/property.hpp>
#include <statefs/consumer.hpp>
#include <cor/util.hpp>
#include <cor/udev.hpp>
#include <cor/error.hpp>
#include <cor/trace.hpp>

template <typename T>
class ChangingValue
{
public:
    ChangingValue(T const & initial)
        : prev_(initial), now_(initial)
    {}

    ChangingValue(T && initial)
        : prev_(std::move(initial)), now_(prev_)
    {}

    ChangingValue(ChangingValue<T> && from)
        : prev_(std::move(from.prev_)), now_(std::move(from.now_))
    {}

    ChangingValue(ChangingValue<T> const &from)
        : prev_(from.prev_), now_(from.now_)
    {}

    ChangingValue& operator = (ChangingValue<T> const &from)
    {
        ChangingValue<T> tmp(from);
        std::swap(*this, tmp);
        return *this;
    }

    ChangingValue& operator = (ChangingValue<T> &&from)
    {
        ChangingValue<T> tmp(std::move(from));
        std::swap(*this, tmp);
        return *this;
    }

    bool changed() const { return now_ != prev_; }

    T last() { return now_; }
    T const & last() const { return now_; }
    T previous() { return prev_; }
    T const & previous() const { return prev_; }

    void set(T const &v) { now_ = v; }
    void set(T &&v) { now_ = std::move(v); }

    template <typename FnT>
    void on_changed(FnT fn)
    {
        if (changed())
            fn(prev_, now_);
    }

    template <typename FnT>
    void on_changed(FnT fn) const
    {
        if (changed())
            fn(prev_, now_);
    }

    void update() { prev_ = now_; }

private:
    T prev_;
    T now_;
};

void update_all() {}

template <typename T, typename... Args>
void update_all(T &v, Args&&... args)
{
    v.update();
    update_all(std::forward<Args>(args)...);
}

template <typename T>
struct LastN
{
    LastN(size_t max_size, T precision)
        : max_size_(max_size)
        , sum_(0)
        , precision_(precision)
    {
    }

    void clear()
    {
        values_.clear();
        sum_ = 0;
    }

    void push(T v)
    {
        values_.push_back(v);
        if (values_.size() > max_size_) {
            sum_ -= values_.front();
            values_.pop_front();
        }
        sum_ += v;
    }

    T average() const
    {
        T sz = values_.size();
        return sz ? (((sum_ * precision_) / sz) / precision_) : sum_;
    }
private:

    size_t max_size_;
    T sum_;
    T precision_;
    std::list<T> values_;
};

namespace asio = boost::asio;
namespace udevpp = cor::udevpp;

using statefs::PropertyStatus;

namespace statefs { namespace udev {

static cor::debug::Log log{"statefs_udev", std::cerr};

std::string env_get(std::string const &name, std::string const &def_val)
{
    auto p = ::getenv(name.c_str());
    return p ? std::string(p) : def_val;
}

long env_get(std::string const &name, long def_val, int base = 10)
{
    auto p = ::getenv(name.c_str());
    try {
        return p ? std::stol(p, nullptr, base) : def_val;
    } catch(std::exception const &e) {
        return def_val;
    }
}

static std::string str_or_default(char const *v, char const *defval)
{
    return v ? v : defval;
}

template <typename T>
T attr(char const *v);

template <>
std::string attr<std::string>(char const *v)
{
    return str_or_default(v, "");
}

template <>
long attr<long>(char const *v)
{
    return atoi(str_or_default(v, "0").c_str());
}

template <>
bool attr<bool>(char const *v)
{
    return attr<long>(v);
}

template <typename T>
static inline std::string statefs_attr(T const &v)
{
    return std::to_string(v);
}

static inline std::string statefs_attr(bool v)
{
    return std::to_string(v ? 1 : 0);
}

static inline std::string const& statefs_attr(std::string const &v)
{
    return v;
}

static inline std::string statefs_attr(char const *v)
{
    return std::string(v);
}

enum class BatteryLevel {
    First_ = 0, Unknown = First_, Empty, Low, Normal, Last_ = Normal
};

static char const * get_level_name(BatteryLevel t)
{
    static char const * names[] = {
        "unknown", "empty", "low", "normal"
    };
    static_assert(sizeof(names)/sizeof(names[0])
                  == cor::enum_size<BatteryLevel>()
                  , "Check battery level names");
    return names[cor::enum_index(t)];
}

enum class ChargingState {
    First_ = 0, Unknown = First_, Charging, Discharging, Idle, Last_ = Idle
};

static char const * get_chg_state_name(ChargingState v)
{
    static char const * names[] = {
        "unknown", "charging", "discharging", "idle"
    };
    static_assert(sizeof(names)/sizeof(names[0])
                  == cor::enum_size<BatteryLevel>()
                  , "Check charging state names");
    return names[cor::enum_index(v)];
}

enum class ChargerType {
    First_ = 0, Absent = First_, DCP, CDP, USB, Mains, Unknown, Last_ = Unknown
};

static char const * get_chg_type_name(ChargerType t)
{
    static char const * names[] = {
        "", "dcp", "cdp", "usb", "dcp", "unknown"
    };
    static_assert(sizeof(names)/sizeof(names[0])
                  == cor::enum_size<ChargerType>()
                  , "Check charger type names");
    return names[cor::enum_index(t)];
}

static ChargerType charger_type(std::string const &v)
{
    return (v == "USB_DCP"
            ? ChargerType::DCP
            : (v == "Mains"
               ? ChargerType::Mains
               : (v == "USB"
                  ? ChargerType::USB
                  : (v == "DCP"
                     ? ChargerType::CDP
                     : ChargerType::Unknown))));
}

// mAh * mV
static const long energy_full_default = 2200 * 3800;

// mA * mV / (sec per hour)
static const long denergy_max_default = 2000 * 3800 / 3600;


class BatteryInfo
{
public:
    BatteryInfo()
        : updated_on(::time(nullptr))
        , energy_now(energy_full_default)
        , capacity_from_energy(42)
        , capacity(42)
        , voltage(3800)
        , current(0)
        , temperature(200) // °C * 10
        , status("")
        , time_to_full(0)
        , time_to_low(3600)
        , level(BatteryLevel::Unknown)
        , power(denergy_max_default)
        , energy_full_(energy_full_default)
        , denergy_max_(denergy_max_default)
        , empty_capacity_(env_get("BATTERY_EMPTY_LIMIT", 3))
        , low_capacity_(env_get("BATTERY_LOW_LIMIT", 10))
        , denergy_(6, 10)
        , path_("")
    {
        calculate_power_limits();
    }

    void setup(udevpp::Device &&new_dev)
    {
        dev_ = cor::make_unique<udevpp::Device>(std::move(new_dev));
        energy_full_ = attr<long>(dev_->attr("energy_full"));
        path_ = attr<std::string>(dev_->path());
        log.info("Battery: ", path_, ", full @ ", energy_full_);
        calculate_power_limits();
    }

    void set_denergy_now(long de);

    void update(udevpp::Device &&);

    long denergy_max() const
    {
        return denergy_max_;
    }

    void calculate(bool);

    void on_charging_changed(ChargingState);

    void on_processed()
    {
        update_all(updated_on, energy_now, capacity_from_energy, capacity
                   , voltage, current, temperature, status
                   , time_to_full, time_to_low, level, power);
    }

    long get_next_timeout() const;

    ChangingValue<time_t> updated_on;
    ChangingValue<long> energy_now;
    ChangingValue<double> capacity_from_energy;
    ChangingValue<long> capacity;
    ChangingValue<long> voltage;
    ChangingValue<long> current;
    ChangingValue<long> temperature;
    ChangingValue<std::string> status;
    ChangingValue<long> time_to_full;
    ChangingValue<long> time_to_low;
    ChangingValue<BatteryLevel> level;
    ChangingValue<long> power;

private:

    void calculate_power_limits()
    {
        mw_per_percent_ = energy_full_ / 100;
        sec_per_percent_max_ = std::max(mw_per_percent_ / denergy_max_, (long)1);
        log.debug("Battery power limits (mw/%, s/%): "
                  , mw_per_percent_
                  , sec_per_percent_max_);
    }

    std::unique_ptr<udevpp::Device> dev_;
    long energy_full_;
    long denergy_max_;
    long empty_capacity_;
    long low_capacity_;
    LastN<long> denergy_;
    std::string path_;

    long mw_per_percent_;
    long sec_per_percent_max_;
    long dtime_;
};

struct ChargerInfo
{
    ChargerInfo(udevpp::Device const &dev)
        : type(charger_type(attr<std::string>(dev.attr("type"))))
        , online(attr<bool>(dev.attr("online")))
    {}

    ChargerType type;
    bool online;
};

class ChargingInfo
{
public:

    ChargingInfo()
        : charger_type(ChargerType::Unknown)
        , is_online(false)
        , state(ChargingState::Unknown)
    {}

    void clear()
    {
        chargers_.clear();
    }

    void update_online_status();
    void on_charger(udevpp::Device &&);
    bool maybe_charger_removed(udevpp::Device const &);

    void calculate(BatteryInfo &, bool);

    void on_processed()
    {
        update_all(charger_type, is_online, state);
    }

    ChangingValue<ChargerType> charger_type;
    ChangingValue<bool> is_online;
    ChangingValue<ChargingState> state;

private:
    std::map<std::string, ChargerInfo> chargers_;
};

class BasicSource : public PropertySource
{
public:
    typedef std::function<std::string()> source_type;

    BasicSource(source_type const &src)
        : src_(src)
    {}

    virtual statefs_ssize_t size() const
    {
        return src_().size();
    }

    virtual std::string read() const
    {
        return src_();
    }

    static std::unique_ptr<BasicSource> create(source_type const &src)
    {
        return cor::make_unique<BasicSource>(src);
    }

private:
    source_type src_;
};


using statefs::consumer::try_open_in_property;


class Monitor;

class BatteryNs : public statefs::Namespace
{
public:

    /**
     * @addtogroup statefs_properties
     *
     * @section battery_ns Battery namespace properties:
     *
     * - ChargePercentage [0, 100] - battery charge percentage
     *
     * - OnBattery [0, 1] - is charger disconnected
     *
     * - IsCharging [0, 1] - is battery really charging (gets power)
     *
     * - LowBattery [0, 1] - is battery level below low battery
     *   threshold (defined by BATTERY_LOW_LIMIT) environment variable
     *
     * - TimeUntilLow (sec) - approx. time until battery will be empty
     *
     * - TimeUntilFull (sec) - approx. time until battery will be charged
     *
     * - Temperature (integer, °C * 10) - battery zone temperature if provided
     *
     * - Power (integer, mW) - average power consumed during several
     *   last measurements (positive - charging)
     *
     * - State (deprecated, string) [unknown, charging, discharging, full, low,
     *    empty] - battery state
     *
     * - Voltage (uV) - battery voltage
     *
     * - Current (uA) - battery current (positive - charging)
     *
     * - ChargerType (string) [usb, dcp, unknown] - charger type ("" - if
     *   absent)
     */
    enum class Prop {
        ChargePercentage, Capacity, OnBattery, LowBattery
            , TimeUntilLow, TimeUntilFull, IsCharging, Temperature
            , Power, State, Voltage, Current, Level
            , ChargerType, ChargingState
            , EOE // end of enum
    };
    enum class PType { Analog, Discrete };

    typedef std::tuple<char const*, char const*, PType> info_item_type;
    static const size_t prop_count = static_cast<size_t>(Prop::EOE);
    typedef std::array<info_item_type, prop_count> info_type;

    static const info_type info;

    typedef std::map<Prop, BasicSource::source_type> analog_info_type;

    BatteryNs();

    virtual ~BatteryNs() {
        io_.stop();
        if (monitor_thread_)
            monitor_thread_->join();
    }

    virtual void release() { }

    void set(Prop, std::string const &);

private:

    template <typename T>
    void insert_(T const &t, size_t pos)
    {
        typename T::handle_ptr prop;
        std::tie(prop, setters_[pos]) = make_prop(t);
        *this << prop;
    }

    asio::io_service io_;
    std::unique_ptr<Monitor> mon_;
    analog_info_type analog_info_;
    std::array<statefs::setter_type, prop_count> setters_;
    std::unique_ptr<std::thread> monitor_thread_;
};

struct SystemState
{
    enum class Screen { Unknown, On, Off, Lost };
    SystemState()
        : screen_(Screen::Unknown)
        , events_count_(0)
    {}

    enum class Event { First_ = 0
            , Timer = First_, Screen, Device
            , Last_ = Device };
    void on_event(Event, boost::system::error_code ec);

    Screen screen_;
    long events_count_;
};

class Monitor
{
    typedef time_t time_type;

    enum TimerAction {
        RestartTimer, NoTimerAction
    };

    enum class TimerStatus {
        Idle, Scheduled, Cancelled
    };

public:
    enum class BatState {
        Unknown, Charging, Discharging, Full, Low
            , Last_ = Low
            };
    Monitor(asio::io_service &io, BatteryNs *);
    void run();

private:

    void start_monitor_blanked();

    template <BatteryNs::Prop Id, typename T>
    void set_battery_prop(T const &v)
    {
        bat_ns_->set(Id, statefs_attr(v));
    }

    template <typename FnT>
    void for_each_power_device(FnT const &fn)
    {
        log.debug("Check each power device");
        before_enumeration();
        udevpp::Enumerate e(root_);
        e.subsystem_add("power_supply");
        auto devs = e.devices();
        devs.for_each([this, &fn](udevpp::DeviceInfo const &info) {
                fn(udevpp::Device{root_, info.path()});
            });
        after_enumeration();
    }

    template <BatteryNs::Prop P, typename T>
    void set(ChangingValue<T> const &src, bool is_set_anyway)
    {
        if (src.changed() || is_set_anyway)
            set_battery_prop<P>(src.last());
    }

    template <BatteryNs::Prop P, typename T, typename FnT>
    void set(ChangingValue<T> const &src, FnT convert, bool is_set_anyway)
    {
        if (src.changed() || is_set_anyway)
            set_battery_prop<P>(convert(src.last()));
    }

    void monitor_events();

    void before_enumeration();
    void on_device(udevpp::Device &&dev);
    void after_enumeration();
    void notify(bool is_initial = false);
    void update_info();
    void monitor_timer();
    void monitor_screen(TimerAction);
    void timer_cancel();

    BatteryNs *bat_ns_;
    asio::io_service &io_;
    size_t dtimer_sec_;
    BatteryInfo battery_;
    ChargingInfo charging_;
    udevpp::Root root_;
    udevpp::Monitor mon_;
    asio::posix::stream_descriptor udev_stream_;
    asio::posix::stream_descriptor blanked_stream_;
    asio::deadline_timer timer_;
    SystemState system_;
    TimerStatus timer_status_;
};

using std::make_tuple;

template <typename T>
std::tuple<typename T::handle_ptr, statefs::setter_type> make_prop(T const &t)
{
    auto prop = statefs::create(t);
    return make_tuple(prop, setter(prop));
}

const BatteryNs::info_type BatteryNs::info = {{
        make_tuple("ChargePercentage", "42", PType::Discrete)
        , make_tuple("Capacity", "42", PType::Discrete)
        , make_tuple("OnBattery", "1", PType::Discrete)
        , make_tuple("LowBattery", "0", PType::Discrete)
        , make_tuple("TimeUntilLow", "7117", PType::Discrete)
        , make_tuple("TimeUntilFull", "0", PType::Discrete)
        , make_tuple("IsCharging", "0", PType::Discrete)
        , make_tuple("Temperature", "293", PType::Discrete)
        , make_tuple("Power", "0", PType::Discrete)
        , make_tuple("State", "unknown", PType::Discrete)
        , make_tuple("Voltage", "3800000", PType::Discrete)
        , make_tuple("Current", "0", PType::Discrete)
        , make_tuple("Level", get_level_name(BatteryLevel::Unknown), PType::Discrete)
        , make_tuple("ChargerType", get_chg_type_name(ChargerType::Unknown), PType::Discrete)
        , make_tuple("ChargingState", get_chg_state_name(ChargingState::Unknown), PType::Discrete)
    }};

class Provider;
static Provider *provider = nullptr;

class Provider : public statefs::AProvider
{
public:
    Provider(statefs_server *server)
        : AProvider("udev", server)
    {
        auto ns = std::make_shared<BatteryNs>();
        insert(std::static_pointer_cast<statefs::ANode>(ns));
    }
    virtual ~Provider() {}

    virtual void release() {
        if (this == provider) {
            delete provider;
            provider = nullptr;
        }
    }

private:
};

// ----------------------------------------------------------------------------

void ChargingInfo::on_charger(udevpp::Device &&dev)
{
    auto path = attr<std::string>(dev.path());
    auto is_online = attr<bool>(dev.attr("online"));
    log.debug("On charger ", path, (is_online ? " online" : " offline"));
    ChargerInfo info(dev);
    auto it = chargers_.find(path);
    if (it != chargers_.end())
        it->second = std::move(info);
    else
        chargers_.insert(std::make_pair(path, std::move(info)));
}

bool ChargingInfo::maybe_charger_removed(udevpp::Device const &dev)
{
    auto path = attr<std::string>(dev.path());
    auto it = chargers_.find(path);
    auto is_removed = false;
    if (it != chargers_.end()) {
        log.info("Charger removed: ", path);
        chargers_.erase(it);
    }
    return is_removed;
}

void ChargingInfo::update_online_status()
{
    auto compare = [](ChargerType const &t1, ChargerType const &t2) {
        return cor::enum_index(t1) > cor::enum_index(t2);
    };
    std::priority_queue<ChargerType, std::vector<ChargerType>, decltype(compare)>
        found_online(compare);

    auto check_online = [&found_online]
        (std::pair<std::string, ChargerInfo> const &nv) {
        auto const &info = nv.second;
        log.debug("Charger ", nv.first, " type=", get_chg_type_name(info.type)
                  , " is_online?=", info.online);
        if (info.online)
            found_online.push(info.type);
    };
    log.debug("Processing chargers");
    std::for_each(chargers_.begin(), chargers_.end(), check_online);
    charger_type.set(found_online.size() > 0
                     ? found_online.top()
                     : ChargerType::Absent);
    if (charger_type.changed()) {
        auto t = charger_type.last();
        log.info("Charger changed:" , get_chg_type_name(t));
        is_online.set(t != ChargerType::Absent);
    }
}

void ChargingInfo::calculate(BatteryInfo &battery, bool is_recalculate)
{
    if (battery.status.changed() || is_recalculate) {
        auto s = battery.status.last();
        log.debug("Battery status changed: ", s);
        state.set(s == "Charging"
                  ? ChargingState::Charging
                  : (s == "Discharging"
                     ? ChargingState::Discharging
                     : (s == "Full"
                        ? ChargingState::Idle
                        : (s == "Not charging"
                           ? ChargingState::Discharging
                           : ChargingState::Unknown))));
        battery.on_charging_changed(state.last());
    }
}

long BatteryInfo::get_next_timeout() const
{
    auto p = power.last();
    auto dt = p ? std::max(sec_per_percent_max_ / 2, (long)1) : 5;
    dt = std::min((int)dt, 60);
    log.debug("dTcalc=", dt);
    return dt;
}

void BatteryInfo::set_denergy_now(long de)
{
    auto enow = energy_now.last();
    log.debug("dE=", de);
    if (!de)
        return;
    denergy_.push(de);
    if (de < 0) {
        denergy_max_ = - de;
        de = denergy_.average();
        // be pessimistic
        if (de < 0 && -de > denergy_max_)
            denergy_max_ = - de;
        calculate_power_limits();
        log.debug("dEavg=", de);
        // hour - 3600s
        auto et = de < 0 ? - enow / de * 360 / 100 : 0;
        time_to_low.set(et);
        time_to_full.set(0);
    } else {
        de = denergy_.average();
        // hour - 3600s
        auto et = de > 0 ? (energy_full_ - enow) / de * 360 / 100 : 0;
        time_to_low.set(0);
        time_to_full.set(et);
    }
    power.set(-de);
}

void BatteryInfo::update(udevpp::Device &&from_dev)
{
    if (!dev_ || *dev_ != from_dev) {
        log.info("Setup new battery ", from_dev.path());
        if (dev_)
            log.warning("Instead of previous battery ", dev_->path());
        setup(std::move(from_dev));
    } else {
        dev_.reset(new udevpp::Device(std::move(from_dev)));
    }
    if (!dev_) {
        log.error("Battery is null, do not update battery info");
        return;
    }
    updated_on.set(::time(nullptr));

    energy_now.set(attr<long>(dev_->attr("energy_now")));
    current.set(attr<long>(dev_->attr("current_now")));
    voltage.set(attr<long>(dev_->attr("voltage_now")));
    temperature.set(attr<long>(dev_->attr("temp")));
    status.set(dev_->attr("status"));
    capacity.set(attr<long>(dev_->attr("capacity")));
}

void BatteryInfo::calculate(bool is_recalculate)
{
    dtime_ = updated_on.last() - updated_on.previous();

    // do not use energy_now for power calc, on some android devices
    // it is wrong or changes rarely
    auto i = current.last(), v = voltage.last();
    auto p = (i / -1000) * (v / 1000) / 1000;
    log.debug("Calc power I*V (", i, "*", v, ")=", p);
    set_denergy_now(p);

    if (energy_now.changed() || is_recalculate)
        capacity_from_energy.set((double)energy_now.last() / energy_full_ * 100);

    // TODO: 2 different sources of capacity allow to compare and
    // verify driver data
    if (capacity.changed() || is_recalculate) {
        auto c = capacity.last();
        if (c < 0 || c > 100) {
            log.warning("Invalid capacity ", c, ", using fake");
            capacity.set(42);
            level.set(BatteryLevel::Unknown);
        } else if (c < empty_capacity_) {
            level.set(BatteryLevel::Empty);
        } else if (c < low_capacity_) {
            level.set(BatteryLevel::Low);
        } else {
            level.set(BatteryLevel::Normal);
        }
    }
    if (temperature.changed() || is_recalculate) {
        auto t = temperature.last();
        if (t < (- 100 * 10) || t > (200 * 10))
            log.warning("Unreasonable temperature value:", t);
    }
}

void BatteryInfo::on_charging_changed(ChargingState)
{
    denergy_.clear();
}

BatteryNs::BatteryNs()
    : Namespace("Battery")
    , mon_(new Monitor(io_, this))
    , analog_info_{{
        // BatteryNs::Prop::Temperature, mon_->temperature_source()
            }}
{
    auto analog_setter = [](std::string const &) {
        throw cor::Error("Analog property can't be set");
        return PropertyStatus::Unchanged;
    };
    //*this << DiscreteProperty("");
    for (size_t i = 0; i < prop_count; ++i) {
        char const *name;
        char const *defval;
        PType ptype;
        std::tie(name, defval, ptype) = info[i];
        if (ptype == PType::Discrete) {
            auto prop = statefs::create(statefs::Discrete{name, defval});
            setters_[i] = setter(prop);
            *this << prop;
        } else {
            auto src = BasicSource::create(analog_info_[static_cast<Prop>(i)]);
            auto prop = statefs::create
                (statefs::Analog{name, defval}, std::move(src));
            setters_[i] = analog_setter;
            *this << prop;
        }
    }
    mon_->run();
    monitor_thread_ = cor::make_unique<std::thread>([this]() { io_.run(); });
}

void BatteryNs::set(Prop id, std::string const &v)
{
    setters_[static_cast<size_t>(id)](v);
}

Monitor::Monitor(asio::io_service &io, BatteryNs *bat_ns)
    : bat_ns_(bat_ns)
    , io_(io)
    , dtimer_sec_(2)
    , root_()
    , mon_([this]() {
            if (!root_)
                throw cor::Error("Root is not initialized");
            return udevpp::Monitor(root_, "power_supply", nullptr);
        }())
    , udev_stream_(io, [this]() {
            auto fd = mon_.fd();
            if (fd < 0)
                throw cor::Error("Monitor fd is invalid");
            return fd;
        }())
    , blanked_stream_(io)
    , timer_(io)
    , timer_status_(TimerStatus::Idle)
{
    log.debug("New monitor");
}

void Monitor::start_monitor_blanked()
{
    static const std::string blanked_name = "Screen.Blanked";
    if (env_get("STATEFS_UDEV_MONITOR_SCREEN", 1)) {
        log.info("Monitor screen blanked state");
        auto blanked_fd = try_open_in_property(blanked_name);
        if (blanked_fd.is_valid()) {
            if (blanked_stream_.is_open()) {
                log.info("Reopening screen descriptor");
                blanked_stream_.close();
            }
            blanked_stream_.assign(blanked_fd.release());
        }
    } else {
        log.warning("Can't open", blanked_name);
    }
}

void Monitor::run()
{
    auto on_device_initial = [this](udevpp::Device &&dev) {
        on_device(std::move(dev));
    };

    start_monitor_blanked();
    for_each_power_device(on_device_initial);
    notify(true);
    monitor_events();
    monitor_screen(NoTimerAction);
}

void SystemState::on_event(SystemState::Event e, boost::system::error_code ec)
{
    static const char * names[] = {
        "Timer", "Screen", "Device"
    };
    static_assert(sizeof(names)/sizeof(names[0])
                  == cor::enum_size<SystemState::Event>()
                  , "Check event names");
    log.debug("Event #", ++events_count_
              , " from ", names[cor::enum_index(e)]
              , " is ", ec);
}

void Monitor::timer_cancel()
{
    if (timer_status_ == TimerStatus::Scheduled) {
        log.debug("Cancel timer");
        timer_status_ = TimerStatus::Cancelled;
        timer_.cancel();
    }
}

void Monitor::monitor_events()
{
    using boost::system::error_code;
    log.debug("Mon Events");
    auto on_event = [this](error_code ec, std::size_t) {
        system_.on_event(SystemState::Event::Device, ec);
        if (ec) {
            log.error("Event is error", ec, ", stopping I/O");
            io_.stop();
            return;
        }
        timer_cancel();
        //before_enumeration();
        on_device(mon_.device(root_));
        after_enumeration();
        notify();
        monitor_events();
    };

    monitor_timer();

    using namespace std::placeholders;
    auto event_wrapper = std::bind
        (cor::error_trace_nothrow
         <decltype(on_event), error_code, std::size_t>
         , on_event, _1, _2);
    udev_stream_.async_read_some(asio::null_buffers(), event_wrapper);
}

void Monitor::monitor_screen(TimerAction timer_action)
{
    using boost::system::error_code;
    auto on_screen = [this](error_code ec, std::size_t) {
        system_.on_event(SystemState::Event::Screen, ec);
        if (ec) {
            system_.screen_ = SystemState::Screen::Lost;
            log.error("Event is error", ec);
            return;
        }
        timer_cancel();
        char buf[4];
        lseek(blanked_stream_.native_handle(), 0, SEEK_SET);
        std::size_t len = 0;
        try {
            len = blanked_stream_.read_some(asio::buffer(buf, sizeof(buf)));
        } catch (std::exception const &e) {
            // can happen during deep sleep, try to reopen
            auto msg = e.what();
            log.warning("Can't read data from blanked: ", msg ? msg : "?");
        }
        if (len && len < sizeof(buf)) { // expecting values [0, 1]
            buf[len] = 0;
            bool is_on = (::atoi(buf) == 0);
            log.debug("Screen is_on?=", is_on);
            system_.screen_ = is_on
                ? SystemState::Screen::On
                : SystemState::Screen::Off;
            update_info();
        } else {
            log.debug("Wrong read from screen?:", len);
            start_monitor_blanked();
        }
        monitor_screen(RestartTimer);
    };

    if (timer_action == RestartTimer)
        monitor_timer();

    if (!blanked_stream_.is_open())
        return;

    using namespace std::placeholders;
    auto screen_wrapper = std::bind
        (cor::error_trace_nothrow<decltype(on_screen), error_code, std::size_t>
         , on_screen, _1, _2);
    blanked_stream_.async_read_some(asio::null_buffers(), screen_wrapper);
}

void Monitor::before_enumeration()
{
    charging_.clear();
}

void Monitor::after_enumeration()
{
    charging_.update_online_status();
}

void Monitor::on_device(udevpp::Device &&dev)
{
    auto t = attr<std::string>(dev.attr("type"));
    if (t == "Battery") {
        // TODO there can be several batteries including also backup battery
        battery_.update(std::move(dev));
    } else if (charger_type(t) != ChargerType::Unknown) {
        charging_.on_charger(std::move(dev));
    } else {
        if (!charging_.maybe_charger_removed(dev)) {
            log.warning("Device of unknown type ", t, ": ", dev.path());
        }
    }
}

void Monitor::notify(bool is_initial)
{
    battery_.calculate(is_initial);
    charging_.calculate(battery_, is_initial);
    typedef BatteryNs::Prop P;

    auto get_is_charging = [this](long p)
    {
        return charging_.charger_type.last() != ChargerType::Absent
            && charging_.state.last() == ChargingState::Charging
            && p > 0;
    };
    auto get_on_battery = [](ChargerType t)
    {
        return (t == ChargerType::Absent || t == ChargerType::Unknown);
    };

    set<P::Capacity>(battery_.capacity_from_energy, is_initial);
    set<P::ChargePercentage>(battery_.capacity, is_initial);
    set<P::OnBattery>(charging_.charger_type, get_on_battery, is_initial);
    set<P::TimeUntilLow>(battery_.time_to_low, is_initial);
    set<P::TimeUntilFull>(battery_.time_to_full, is_initial);
    set<P::IsCharging>(battery_.power, get_is_charging, is_initial);
    set<P::Temperature>(battery_.temperature, is_initial);
    set<P::Power>(battery_.power, is_initial);
    if (battery_.level.changed() || charging_.state.changed()) {
        auto charging_state = charging_.state.last();
        auto bat_level = battery_.level.last();
        std::string res{""};
        switch (charging_state) {
        case ChargingState::Charging:
            if (bat_level == BatteryLevel::Low || bat_level == BatteryLevel::Empty)
                res = "low";
            else
                res = "charging";
            break;
        case ChargingState::Idle:
            res = "full";
            break;
        default:
            switch (bat_level) {
            case BatteryLevel::Empty:
                res = "empty";
                break;
            case BatteryLevel::Low:
                res = "low";
                break;
            case BatteryLevel::Normal:
                res = "discharging";
                break;
            default:
                res = "unknown";
                break;
            }
            break;
        }
        set_battery_prop<P::State>(res);
    }
    set<P::Voltage>(battery_.voltage, is_initial);
    set<P::Current>(battery_.current, is_initial);
    set<P::Level>(battery_.level, get_level_name, is_initial);
    set<P::ChargerType>(charging_.charger_type, get_chg_type_name, is_initial);
    set<P::ChargingState>(charging_.state, get_chg_state_name, is_initial);

    if (!charging_.state.changed()) {
        auto charging_now = charging_.state.last();
        if (charging_now == ChargingState::Charging
            || charging_now == ChargingState::Idle) {
            static const decltype(dtimer_sec_) dt_charging = 10;
            log.debug("Online, so poll each ", dt_charging);
            dtimer_sec_ = dt_charging;
        } else {
            dtimer_sec_ = battery_.get_next_timeout();
        }
    } else {
        log.debug("Online status is changed");
        dtimer_sec_ = 5;
    }
    battery_.on_processed();
    charging_.on_processed();
}

void Monitor::update_info()
{
    using namespace std::placeholders;
    for_each_power_device(std::bind(&Monitor::on_device, this, _1));
    notify();
}

void Monitor::monitor_timer()
{
    log.debug("Monitor Timer");
    auto handler = [this](boost::system::error_code ec) {
        system_.on_event(SystemState::Event::Timer, ec);
        if (ec) {
            if (ec == asio::error::operation_aborted) {
                // if (timer_status_ == TimerStatus::Cancelled) {
                log.debug("Timer is cancelled from Monitor");
                timer_status_ = TimerStatus::Idle;
                return;
                // }
            } else {
                log.critical("Timer error event ", ec);
                io_.stop();
                return;
            }
        }
        if (system_.screen_ == SystemState::Screen::Lost)
            monitor_screen(NoTimerAction);
        update_info();
        monitor_timer();
    };
    auto wrapper = [handler](boost::system::error_code ec) {
        try {
            handler(ec);
        } catch(std::exception const &e) {
            log.error("Caught exception: ", e.what());
        }
    };

    timer_status_ = TimerStatus::Scheduled;
    timer_.expires_from_now(boost::posix_time::seconds(dtimer_sec_));
    timer_.async_wait(wrapper);
}


static inline Provider *init_provider(statefs_server *server)
{
    if (provider)
        throw std::logic_error("provider ptr is already set");
    provider = new Provider(server);
    return provider;
}


}}

EXTERN_C struct statefs_provider * statefs_provider_get
(struct statefs_server *server)
{
    return statefs::udev::init_provider(server);
}
