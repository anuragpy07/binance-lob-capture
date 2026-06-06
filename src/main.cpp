#include "common/signal_handler.hpp" // g_stop for replay mode
#include "common/time_utils.hpp"
#include "common/constants.hpp"
#include "csv/market_data_writer.hpp"
#include "csv/orderbook_writer.hpp"
#include "orderbook/lob_engine.hpp"
#include "metrics/metrics.hpp"
#include "replay/replay_engine.hpp"
#include "websocket/connection_manager.hpp"
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bncap {

// ---------------------------------------------------------------------------
// CLI parsing
// ---------------------------------------------------------------------------
struct Config {
    std::string venue_str   = "spot";
    std::vector<std::string> symbols;
    std::string output_dir  = ".";
    int         duration_s  = 0;      // 0 = run until signal
    std::string replay_file;          // non-empty = replay mode
};

static Config parse_args(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (++i >= argc) throw std::runtime_error("missing value for " + arg);
            return argv[i];
        };

        if      (arg == "--venue")      cfg.venue_str   = next();
        else if (arg == "--output-dir") cfg.output_dir  = next();
        else if (arg == "--duration")   cfg.duration_s  = std::stoi(next());
        else if (arg == "--replay")     cfg.replay_file = next();
        else if (arg == "--symbols") {
            const std::string s = next();
            std::istringstream ss(s);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                if (!tok.empty()) cfg.symbols.push_back(tok);
            }
        } else {
            std::cerr << "[main] ignoring unknown argument: " << arg << '\n';
        }
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// Writer setup helpers
// ---------------------------------------------------------------------------
static std::string make_md_path(const std::string& output_dir,
                                 const std::string& venue_s,
                                 const std::string& symbol,
                                 const std::string& date_s) {
    return output_dir + "/market_data_" + venue_s + "_" + symbol + "_" + date_s + ".csv";
}

static std::string make_ob_path(const std::string& output_dir,
                                 const std::string& venue_s,
                                 const std::string& symbol,
                                 const std::string& date_s) {
    return output_dir + "/market_data_" + venue_s + "_" + symbol + "_" + date_s + "_orderbook.csv";
}

} // namespace bncap

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    using namespace bncap;
    namespace fs  = std::filesystem;
    namespace net = boost::asio;

    Config cfg;
    try {
        cfg = parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Usage error: " << e.what() << '\n';
        return 1;
    }

    Metrics metrics;
    net::io_context ioc;

    // -------------------------------------------------------------------
    // Replay mode
    // -------------------------------------------------------------------
    if (!cfg.replay_file.empty()) {
        fs::create_directories(cfg.output_dir);
        LobEngine engine(metrics);
        ReplayEngine replay(engine, metrics);
        try {
            replay.run(cfg.replay_file, cfg.output_dir);
        } catch (const std::exception& e) {
            std::cerr << "Replay error: " << e.what() << '\n';
            return 1;
        }
        metrics.print_summary();
        return 0;
    }

    // -------------------------------------------------------------------
    // Live capture mode
    // -------------------------------------------------------------------
    if (cfg.symbols.empty()) {
        std::cerr << "Error: specify at least one symbol with --symbols\n";
        return 1;
    }
    if (cfg.symbols.size() > MAX_SYMBOLS) {
        std::cerr << "Error: too many symbols (max " << MAX_SYMBOLS << ")\n";
        return 1;
    }

    const Venue venue =
        (cfg.venue_str == "usdm") ? Venue::usdm : Venue::spot;

    fs::create_directories(cfg.output_dir);

    const RecvTs startup_ts = split_ts(now_ns());
    const std::string date_s = utc_date_str(startup_ts.tsec);

    LobEngine engine(metrics);

    // Create per-symbol writers
    for (const auto& sym : cfg.symbols) {
        SymbolWriters sw;

        sw.md = std::make_unique<MarketDataWriter>();
        sw.md->open(make_md_path(cfg.output_dir, cfg.venue_str, sym, date_s));

        sw.ob = std::make_unique<OrderBookWriter>();
        sw.ob->open(make_ob_path(cfg.output_dir, cfg.venue_str, sym, date_s));

        engine.add_symbol(sym, std::move(sw));
    }

    // Use Boost.Asio signal_set for async-signal-safe SIGINT/SIGTERM handling
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](boost::system::error_code ec, int sig) {
        if (!ec) {
            std::cerr << "[main] signal " << sig << " received, stopping\n";
            ioc.stop();
        }
    });
    // Also set g_stop for replay mode compatibility
    install_signal_handlers();

    // Optional duration timer
    std::shared_ptr<net::steady_timer> duration_timer;
    if (cfg.duration_s > 0) {
        duration_timer = std::make_shared<net::steady_timer>(ioc);
        duration_timer->expires_after(std::chrono::seconds(cfg.duration_s));
        duration_timer->async_wait([&ioc](boost::system::error_code ec) {
            if (!ec) {
                std::cerr << "[main] duration reached, stopping\n";
                ioc.stop();
            }
        });
    }

    ConnectionManager mgr(ioc, venue, cfg.symbols, engine, metrics);
    mgr.start();

    std::cerr << "[main] running (Ctrl-C to stop)\n";
    ioc.run();

    mgr.stop();
    metrics.print_summary();
    return 0;
}