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

namespace asio = boost::asio;
namespace udevpp = cor::udevpp;

using statefs::PropertyStatus;

namespace statefs { namespace udev {

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


enum class ChargerType {
    Absent = 0, DCP, USB, Mains, Unknown, Last_ = Unknown
};

static char const * charger_type_name(ChargerType t)
{
    static char const * charger_type_names[] = {
        "", "dcp", "usb", "dcp", "unknown"
    };
    static_assert(sizeof(charger_type_names)/sizeof(charger_type_names[0])
                  == cor::enum_size<ChargerType>()
                  , "Check charger type names");
    return charger_type_names[cor::enum_index(t)];
}

static ChargerType charger_type(std::string const &v)
{
    return (v == "USB_DCP"
            ? ChargerType::DCP
            : (v == "Mains"
               ? ChargerType::Mains
               : (v == "USB"
                  ? ChargerType::USB
                  : ChargerType::Unknown)));
}

struct ChargerInfo
{
    ChargerInfo(udevpp::Device const &dev)
        : type(charger_type(attr<std::string>(dev.attr("type"))))
        , online(attr<bool>(dev.attr("online")))
    {}

    ChargerType type;
    bool online;
};

enum class Prop
{
    BatTime
        , IsOnline
        , EnergyNow
        , Capacity
        , Voltage
        , Current
        , Power
        , Temperature
        , State
        , TimeToLow
        , TimeToFull
        , Charger
        , Last_ = Charger
};

typedef Record<Prop
               , time_t
               , bool
               , long
               , long
               , long
               , long
               , long
               , long
               , std::string
               , long
               , long
               , ChargerType
               > State;

}}

RECORD_TRAITS_FIELD_NAMES(statefs::udev::State
                          , "BatTime"
                          , "IsOnline"
                          , "EnergyNow"
                          , "Capacity"
                          , "Voltage"
                          , "Current"
                          , "Power"
                          , "Temperature"
                          , "State"
                          , "TimeToLow"
                          , "TimeToFull"
                          , "Charger"
                          );

namespace statefs { namespace udev {

static cor::debug::Log log{"statefs_udev", std::cerr};

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
     * - State (string) [unknown, charging, discharging, full, low,
     *    empty] - battery state
     *
     * - Voltage (uV) - battery voltage
     *
     * - Current (uA) - battery current (positive - charging)
     *
     * - Charger (string) [usb, dcp, unknown] - charger type ("" - if
     *   absent)
     */
    enum class Prop {
        ChargePercentage, Capacity, OnBattery, LowBattery
            , TimeUntilLow, TimeUntilFull, IsCharging, Temperature
            , Power, State, Voltage, Current, Charger
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
    {}

    Screen screen_;
};

class Monitor
{
    typedef time_t time_type;

    enum TimerAction {
        RestartTimer, NoTimerAction
    };
public:
    enum class BatState {
        Unknown, Charging, Discharging, Full, Low
            , Last_ = Low
            };
    Monitor(asio::io_service &io, BatteryNs *);
    void run();

    BasicSource::source_type temperature_source() const
    {
        // TODO
        return [this]() {
            std::string res("-1");
            if (battery_)
                res = str_or_default(battery_->attr("temp"), "-1");
            return res;
        };
    }

private:

    template <BatteryNs::Prop Id, typename T>
    void set_battery_prop(T const &v)
    {
        bat_ns_->set(Id, statefs_attr(v));
    }

    template <BatteryNs::Prop Id, typename T>
    std::function<void(T const &)> battery_setter()
    {
        using namespace std::placeholders;
        return std::bind(&Monitor::set_battery_prop<Id, T>, this, _1);
    }

    void calc_limits()
    {
        sec_per_percent_max_ = std::max(mw_per_percent_ / denergy_max_, (long)1);
    }

    template <Prop i, typename T>
    void set(T v)
    {
        now_.get<i>() = v;
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

    void monitor_events();

    void before_enumeration();
    void on_device(udevpp::Device &&dev);
    void on_charger(udevpp::Device const &dev);
    void on_battery(udevpp::Device const &dev);
    void after_enumeration();
    void notify();
    void update_info();
    void monitor_timer();
    void monitor_screen(TimerAction);

    BatteryNs *bat_ns_;
    asio::io_service &io_;
    size_t dtimer_sec_;
    long energy_full_;
    long denergy_max_;
    long mw_per_percent_;
    long sec_per_percent_max_;
    udevpp::Root root_;
    udevpp::Monitor mon_;
    asio::posix::stream_descriptor udev_stream_;
    asio::posix::stream_descriptor blanked_stream_;
    asio::deadline_timer timer_;
    State previous_;
    State now_;
    LastN<long> denergy_;
    std::unique_ptr<udevpp::Device> battery_;
    std::map<std::string, ChargerInfo> charger_state_;
    bool is_actual_;
    long low_capacity_;
    SystemState system_;
    std::atomic_flag is_timer_allowed_;
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
        , make_tuple("Charger", charger_type_name(ChargerType::Unknown)
                     , PType::Discrete)
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

static long energy_full_default = 800000;

static State state_default{
    (time_t)0
        , false
        , energy_full_default
        , 42 // %
        , 0 // uA
        , 3800 // mV
        , 36000 // uW
        , 273 // oK
        , "unknown"
        , 0 , 0 // s
        , ChargerType::Unknown
        };

Monitor::Monitor(asio::io_service &io, BatteryNs *bat_ns)
    : bat_ns_(bat_ns)
    , io_(io)
    , dtimer_sec_(2)
    , energy_full_(energy_full_default)
    , denergy_max_(10000)
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
    , previous_(state_default)
    , now_(state_default)
    , denergy_(6, 10)
    , is_actual_(false)
    , low_capacity_(env_get("BATTERY_LOW_LIMIT", 10))
    {
        log.debug("New monitor");
        is_timer_allowed_.test_and_set(std::memory_order_acquire);
        set<Prop::BatTime>(::time(nullptr));
    }

void Monitor::run()
{
    auto blanked_fd = try_open_in_property("Screen.Blanked");
    if (blanked_fd.is_valid())
        blanked_stream_.assign(blanked_fd.release());
    auto on_device_initial = [this](udevpp::Device &&dev)
        {
            auto t = str_or_default(dev.attr("type"), "");
            if (t == "Battery") {
                energy_full_ = attr<long>(dev.attr("energy_full"));
                log.debug("FULL=", energy_full_);
            }
            on_device(std::move(dev));
            mw_per_percent_ = energy_full_ / 100;
            log.debug("mW/%=", mw_per_percent_);
            calc_limits();
        };

    for_each_power_device(on_device_initial);
    notify();
    monitor_events();
    monitor_screen(NoTimerAction);
}

void Monitor::monitor_events()
{
    using boost::system::error_code;
    log.debug("Mon Events");
    auto on_event = [this](error_code ec, std::size_t) {
        log.debug("Got event");
        if (ec) {
            log.error("Event is error", ec, ", stopping I/O");
            io_.stop();
            return;
        }
        log.debug("Cancel timer");
        is_timer_allowed_.clear(std::memory_order_release);
        timer_.cancel();
        //before_enumeration();
        on_device(mon_.device(root_));
        after_enumeration();
        notify();
        monitor_events();
    };

    //is_timer_allowed_.test_and_set(std::memory_order_acquire);
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
        log.debug("Got screen event");
        if (ec) {
            system_.screen_ = SystemState::Screen::Lost;
            log.error("Event is error", ec);
            return;
        }
        log.debug("Cancel timer");
        is_timer_allowed_.clear(std::memory_order_release);
        timer_.cancel();
        char buf[4];
        lseek(blanked_stream_.native_handle(), 0, SEEK_SET);
        auto len = blanked_stream_.read_some(asio::buffer(buf, sizeof(buf)));
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
        }
        //is_timer_allowed_.test_and_set(std::memory_order_acquire);
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
    charger_state_.clear();
}

void Monitor::after_enumeration()
{
    auto compare = [](ChargerType const &t1, ChargerType const &t2) {
        return cor::enum_index(t1) > cor::enum_index(t2);
    };
    std::priority_queue<ChargerType, std::vector<ChargerType>, decltype(compare)>
        found_online(compare);

    auto check_online = [&found_online]
        (std::pair<std::string, ChargerInfo> const &nv) {
        auto const &info = nv.second;
        log.debug("Charger ", nv.first, ", type=", charger_type_name(info.type)
                  , "is_online?=", info.online);
        if (info.online)
            found_online.push(info.type);
    };
    log.debug("Check chargers state");
    std::for_each(charger_state_.begin(), charger_state_.end(), check_online);
    auto is_online = found_online.size() > 0;
    if (is_online)
        log.debug("There is online charger:", charger_type_name(found_online.top()));
    set<Prop::Charger>(is_online ? found_online.top() : ChargerType::Absent);
    set<Prop::IsOnline>(is_online);
}

void Monitor::on_device(udevpp::Device &&dev)
{
    auto t = attr<std::string>(dev.attr("type"));
    if (charger_type(t) != ChargerType::Unknown) {
        on_charger(dev);
        // if (!charger_ || *charger_ != dev)
        //     charger_ = cor::make_unique<udevpp::Device>(std::move(dev));
    } else if (t == "Battery") {
        on_battery(dev);
        // TODO there can be several batteries including also backup battery
        if (!battery_ || *battery_ != dev)
            battery_ = cor::make_unique<udevpp::Device>(std::move(dev));
    } else {
        charger_state_.erase(dev.path());
        log.warning("Device of unknown type ", t, ": ", dev.path());
    }
}

void Monitor::on_charger(udevpp::Device const &dev)
{
    auto path = attr<std::string>(dev.path());
    auto is_online = attr<bool>(dev.attr("online"));
    log.debug("On charger ", path, (is_online ? " online" : " offline"));
    ChargerInfo info(dev);
    auto it = charger_state_.find(path);
    if (it != charger_state_.end())
        it->second = std::move(info);
    else
        charger_state_.insert(std::make_pair(path, std::move(info)));
}

void Monitor::on_battery(udevpp::Device const &dev)
{
    auto path = attr<std::string>(dev.path());
    log.debug("On battery ", path);
    set<Prop::BatTime>(::time(nullptr));
    set<Prop::EnergyNow>(attr<long>(dev.attr("energy_now")));
    set<Prop::Current>(attr<long>(dev.attr("current_now")));
    set<Prop::Voltage>(attr<long>(dev.attr("voltage_now")));
    set<Prop::Temperature>(attr<long>(dev.attr("temp")));
    set<Prop::State>(dev.attr("status"));
    set<Prop::Capacity>(attr<long>(dev.attr("capacity")));
}

void Monitor::notify()
{
    typedef BatteryNs::Prop P;

    time_type dt = 0;
    auto update_dt = [&dt](time_type const& tnow, time_type const& twas) {
        dt = tnow - twas;
    };

    bool is_energy_changed = false;
    bool is_charging_changed = false;
    bool is_bat_low = false;

    auto process_is_online = [this, &is_charging_changed]
        (bool online_now, bool) {
        log.info("Charger online status ->", online_now);
        is_charging_changed = true;
        set_battery_prop<P::OnBattery>(!online_now);
    };

    auto process_de = [this, &is_energy_changed](long de) {
        auto enow = now_.get<Prop::EnergyNow>();
        log.debug("dE=", de);
        if (!de)
            return;
        denergy_.push(de);
        if (de < 0) {
            denergy_max_ = -de;
            de = denergy_.average();
            if (de < 0 && -de > denergy_max_)
                denergy_max_ = -de; // be pessimistic
            calc_limits();
            log.debug("dEavg=", de);
            auto et = de != 0 ? - enow / de * 360 / 100 : 0;
            set<Prop::TimeToLow>(et);
            set<Prop::TimeToFull>(0);
        } else {
            de = denergy_.average();
            // hour - 3600s
            auto et = de != 0 ? (energy_full_ - enow) / de * 360 / 100 : 0;
            set<Prop::TimeToLow>(0);
            set<Prop::TimeToFull>(et);
        }
        is_energy_changed = true;
        set<Prop::Power>(-de);
    };

    auto process_energy = [this](long enow) {
        set_battery_prop<P::Capacity>((double)enow / energy_full_ * 100);
    };

    auto process_percentage = [this, &is_bat_low](long v) {
        if (v < 0 || v > 100) {
            log.warning("Do not accept percentage ", v, ", use fake");
            v = 42;
        }
        log.debug("Capacity=", v);
        set_battery_prop<P::ChargePercentage>(v);
        is_bat_low = v <= low_capacity_;
        set_battery_prop<P::LowBattery>(is_bat_low);
    };

    auto process_voltage = [this](long v) {
        set_battery_prop<P::Voltage>(v);
        // TODO check it
    };

    auto process_current = [this, &is_energy_changed, &process_de](long i) {
        set_battery_prop<P::Current>(i);
        if (is_energy_changed)
            return;
        // otherwise calculate from I*V
        auto v = now_.get<Prop::Voltage>();
        auto p = (i / -1000) * (v / 1000) / 1000;
        log.debug("Calc power I*V (", i, "*", v, ")=", p);
        process_de(p);
    };

    auto process_power = [this](long p) {
        log.debug("Power=", p);
        set_battery_prop<P::Power>(p);
    };

    auto process_state = [this, &is_bat_low](std::string s) {
        log.debug("Status=", s);
        static const std::map<std::string, std::string> rename = {{
                {"Charging", "charging"}
                , {"Discharging", "discharging"}
                , {"Full", "full"}
            }};
        std::string state{"unknown"};
        auto it = rename.find(s);
        if (it != rename.end()) {
            state = it->second;
        } else {
            if (is_bat_low)
                state = "low";
        }
        set_battery_prop<P::State>(state);
    };

    auto process_charger = [this](ChargerType t) {
        set_battery_prop<P::Charger>(charger_type_name(t));
    };

    // see enum class Prop
    const auto actions = std::make_tuple
        (update_dt
         , process_is_online
         , process_energy
         , process_percentage //battery_setter<P::ChargePercentage, long>()
         , process_voltage
         , process_current
         , process_power
         , battery_setter<P::Temperature, long>()
         , process_state
         , battery_setter<P::TimeUntilLow, long>()
         , battery_setter<P::TimeUntilFull, long>()
         , process_charger
         );

    auto set_charging = [this]() {
        auto p = now_.get<Prop::Power>();
        auto o = now_.get<Prop::IsOnline>();
        set_battery_prop<P::IsCharging>(o && p > 0);
    };
    if (is_actual_) {
        auto count = cor::copy_apply_if_changed
            (previous_.data, now_.data, actions);
        // TODO check what props are changed and return if nothing is
        // changed
        if (is_charging_changed || is_energy_changed)
            set_charging();
    } else {
        set_battery_prop<P::ChargePercentage>(now_.get<Prop::Capacity>());
        process_energy(now_.get<Prop::EnergyNow>());
        auto p = now_.get<Prop::Power>();
        set_battery_prop<P::Power>(p < 0);
        auto o = now_.get<Prop::IsOnline>();
        set_charging();
        set_battery_prop<P::OnBattery>(!o);
        set_battery_prop<P::TimeUntilLow>(now_.get<Prop::TimeToLow>());
        set_battery_prop<P::TimeUntilFull>(now_.get<Prop::TimeToFull>());
        process_state(now_.get<Prop::State>());
        process_charger(now_.get<Prop::Charger>());
        is_actual_ = true;
    }

    if (!is_charging_changed) {
        auto o = now_.get<Prop::IsOnline>();
        if (o) {
            log.debug("Online, so timer period is short");
            dtimer_sec_ = 4;
        } else {
            auto p = now_.get<Prop::Power>();
            // auto c = get<Prop::Capacity>(now_);
            dtimer_sec_ = p ? std::max(sec_per_percent_max_ / 2, (long)1) : 5;
            dtimer_sec_ = std::min((int)dtimer_sec_, 60);
            log.debug("dTcalc=", dtimer_sec_);
        }
    } else {
        log.debug("Online status is changed");
        denergy_.clear();
        dtimer_sec_ = 5;
    }
}

void Monitor::update_info()
{
    // if (!(charger_ && battery_)) {
    using namespace std::placeholders;
    for_each_power_device(std::bind(&Monitor::on_device, this, _1));
    // } else {
    //     on_charger(*charger_);
    //     on_battery(*battery_);
    // }
    notify();
}

void Monitor::monitor_timer()
{
    log.debug("Monitor Timer");
    auto handler = [this](boost::system::error_code ec) {
        log.debug("Timer event");
        if (ec == asio::error::operation_aborted) {
            if (!is_timer_allowed_.test_and_set(std::memory_order_acquire)) {
                log.debug("Timer is cancelled from Monitor");
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
