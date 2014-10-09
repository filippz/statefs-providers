#include <thread>
#include <memory>
#include <array>
#include <functional>
#include <time.h>

#include <boost/asio.hpp>
#include <boost/asio/posix/basic_descriptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>

#include <statefs/property.hpp>
#include <statefs/consumer.hpp>
#include <cor/util.hpp>
#include <cor/udev.hpp>
#include <cor/error.hpp>

namespace asio = boost::asio;
namespace udevpp = cor::udevpp;

using statefs::PropertyStatus;

namespace statefs { namespace udev {

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

template <typename T>
struct LastN
{
    LastN(size_t max_size, T precision)
        : max_size_(max_size_)
        , sum_(0)
        , precision_(precision)
    {}

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
        if (!sz)
            return 0;
        std::cerr << "??" << sum_ << ","
                  << precision_ << "," << sz << "," <<
            (((sum_ * precision_) / sz) / precision_) << std::endl;
        return sz ? (((sum_ * precision_) / sz) / precision_) : sum_;
    }

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

    enum class Prop {
        ChargePercentage, Capacity, OnBattery, LowBattery
            , TimeUntilLow, TimeUntilFull, IsCharging, Temperature
            , Power
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

class Monitor
{
    typedef time_t time_type;

    enum class Prop
    {
        BatTime, IsOnline, EnergyNow, Power
            , Capacity, TimeToLow, TimeToFull
    };

    typedef std::tuple<time_type, bool, long, long
                       , long, long, long> state_type;

    enum TimerAction {
        RestartTimer, NoTimerAction
    };
public:
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
        std::get<static_cast<size_t>(i)>(current_) = v;
    }

    template <Prop I>
    static typename std::tuple_element
    <static_cast<size_t>(I), state_type>::type const& get(state_type const &src)
    {
        return std::get<static_cast<size_t>(I)>(src);
    }

    template <typename FnT>
    void for_each_power_device(FnT const &fn)
    {
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
    state_type last_;
    state_type current_;
    LastN<long> denergy_;
    std::unique_ptr<udevpp::Device> battery_;
    std::map<std::string, bool> charger_state_;
};

using std::make_tuple;

template <typename T>
std::tuple<typename T::handle_ptr, statefs::setter_type> make_prop(T const &t)
{
    auto prop = statefs::create(t);
    return make_tuple(prop, setter(prop));
}


const BatteryNs::info_type BatteryNs::info = {{
        make_tuple("ChargePercentage", "97", PType::Discrete)
        , make_tuple("Capacity", "97", PType::Discrete)
        , make_tuple("OnBattery", "1", PType::Discrete)
        , make_tuple("LowBattery", "0", PType::Discrete)
        , make_tuple("TimeUntilLow", "3600", PType::Discrete)
        , make_tuple("TimeUntilFull", "0", PType::Discrete)
        , make_tuple("IsCharging", "0", PType::Discrete)
        , make_tuple("Temperature", "293", PType::Analog)
        , make_tuple("Power", "0", PType::Discrete)
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
        BatteryNs::Prop::Temperature, mon_->temperature_source()
            }}
{
    auto analog_setter = [](std::string const &v) {
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
    , energy_full_(800000)
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
    , last_{::time(nullptr), false, energy_full_, 0, 100, 36000, 0}
    , current_(last_)
    , denergy_(6, 10)
    {}

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
                std::cerr << "FULL:" << energy_full_ << std::endl;
            }
            on_device(std::move(dev));
            mw_per_percent_ = energy_full_ / 100;
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
    std::cerr << "Mon Events\n";
    auto on_event = [this](error_code ec, std::size_t) {
        std::cerr << "Ev Handler\n";
        timer_.cancel();
        if (ec) {
            std::cerr << "err" << ec << std::endl;
            io_.stop();
            return;
        }
        before_enumeration();
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
        timer_.cancel();
        if (ec) {
            std::cerr << "err" << ec << std::endl;
            io_.stop();
            return;
        }
        char buf[4];
        lseek(blanked_stream_.native_handle(), 0, SEEK_SET);
        auto len = blanked_stream_.read_some(asio::buffer(buf, sizeof(buf)));
        if (len && len < sizeof(buf)) {
            buf[len] = 0;
            if (::atoi(buf))
                update_info();
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
    //charger_state_.clear();
}

void Monitor::after_enumeration()
{
    auto v = std::any_of(charger_state_.begin(), charger_state_.end()
                          , [](std::pair<std::string, bool> const &nv) {
                              return nv.second;
                          });
    set<Prop::IsOnline>(v);
}

void Monitor::on_device(udevpp::Device &&dev)
{
    auto t = str_or_default(dev.attr("type"), "");
    if (t == "Mains" || t == "USB") {
        on_charger(dev);
        // if (!charger_ || *charger_ != dev)
        //     charger_ = cor::make_unique<udevpp::Device>(std::move(dev));
    } else if (t == "Battery") {
        on_battery(dev);
        // TODO there can be several batteris including also backup battery
        if (!battery_ || *battery_ != dev)
            battery_ = cor::make_unique<udevpp::Device>(std::move(dev));
    }
}

void Monitor::on_charger(udevpp::Device const &dev)
{
    auto path = attr<std::string>(dev.path());
    auto is_online = attr<bool>(dev.attr("online"));
    charger_state_.insert({path, is_online});
}

void Monitor::on_battery(udevpp::Device const &dev)
{
    set<Prop::BatTime>(::time(nullptr));
    set<Prop::EnergyNow>(attr<long>(dev.attr("energy_now")));
    set<Prop::Capacity>(attr<long>(dev.attr("capacity")));
}

void Monitor::notify()
{
    typedef BatteryNs::Prop P;

    time_type dt = 0;
    auto update_dt = [&dt](time_type const& tnow, time_type const& twas) {
        dt = tnow - twas;
    };

    auto process_energy = [this, &dt](long enow, long ewas) {
        std::cerr << "Energy -> " << enow << std::endl;
        auto sec = dt;
        std::cerr << "DT:" << sec << std::endl;
        if (!sec)
            return;

        auto de = (enow - ewas) / sec;
        std::cerr << "dE=" << de << std::endl;
        if (!de)
            return;
        denergy_.push(de);
        if (de < 0) {
            if (-de > denergy_max_) {
                denergy_max_ = -de;
                calc_limits();
            }
            de = denergy_.average();
            std::cerr << "avg=" << de << std::endl;
            auto et = - enow / de;
            set<Prop::TimeToLow>(et);
            set<Prop::TimeToFull>(0);
        } else {
            de = denergy_.average();
            auto et = de ? (energy_full_ - enow) / de : 0;
            set<Prop::TimeToLow>(0);
            set<Prop::TimeToFull>(et);
        }
        set<Prop::Power>(de);
    };

    bool is_charging_changed = false;
    auto process_is_online = [this, &is_charging_changed]
        (bool online_now, bool online_was) {
        std::cerr << "Chg -> " << online_now << std::endl;
        if (online_now != online_was) {
            is_charging_changed = true;
            set_battery_prop<P::IsCharging>(online_now);
        }
    };

    auto process_power = [this](long p) {
        set_battery_prop<P::Power>(p);
        set_battery_prop<P::OnBattery>(p < 0);
    };

    // BatTime, IsOnline, EnergyNow
    // , Power, Capacity
    // , TimeToLow, TimeToFull
    const auto actions = std::make_tuple
        (update_dt
         , process_is_online
         , process_energy
         , process_power
         , battery_setter<P::ChargePercentage, long>()
         , battery_setter<P::TimeUntilLow, long>()
         , battery_setter<P::TimeUntilFull, long>());

    auto count = cor::copy_apply_if_changed(last_, current_, actions);
    if (count == 1) {
        return;
    }

    if (is_charging_changed) {
        denergy_.clear();
        dtimer_sec_ = 5;
        return;
    }

    std::cerr << "Changed " << count << std::endl;
    auto o = get<Prop::IsOnline>(current_);
    if (o) {
        std::cerr << "Online" << std::endl;
        dtimer_sec_ = 4;
    } else {
        auto p = get<Prop::Power>(current_);
        // auto c = get<Prop::Capacity>(current_);
        dtimer_sec_ = p ? std::max(sec_per_percent_max_ / 2, (long)1) : 5;
        std::cerr << "dTcalc=" << dtimer_sec_ << std::endl;
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
    std::cerr << "Mon Timer\n";
    auto handler = [this](boost::system::error_code ec) {
        std::cerr << "Timer Handler\n";
        if (ec == asio::error::operation_aborted) {
            std::cerr << "Timer is cancelled\n";
            return;
        }
        update_info();
        monitor_timer();
    };
    auto wrapper = [handler](boost::system::error_code ec) {
        try {
            handler(ec);
        } catch(std::exception const &e) {
            std::cerr << "Caught exception: " << e.what() << std::endl;
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
