
#include <iostream>
#include <memory>

#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/system/system_error.hpp>
#include <boost/core/span.hpp>

#include <fmt/core.h>

#include "DBConnection.h"

DBConnection::DBConnection() : _ssl_ctx(boost::asio::ssl::context::tls_client), _conn(_ctx, _ssl_ctx) {}

DBConnection::~DBConnection() {
    _conn.close();
}

bool DBConnection::connect() {
    static bool logged = false;
    try {
        boost::asio::ip::tcp::resolver resolver(_ctx.get_executor());
        auto endpoints = resolver.resolve("127.0.0.1", boost::mysql::default_port_string);
        boost::mysql::handshake_params params("root", "", "test", boost::mysql::handshake_params::default_collation, boost::mysql::ssl_mode::enable);

        if (!logged) std::cout << "Connecting to mysql server at " << endpoints.begin()->endpoint() << " ... ";

        _conn.connect(*endpoints.begin(), params);
    }
    catch (const boost::mysql::error_with_diagnostics& err) {
        std::cerr << "Error: " << err.what() << '\n'
            << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        return false;
    }
    catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << std::endl;
        return false;
    }
    if (!logged) { logged = true; printf("Connected\n"); }
    return true;
}

bool DBConnection::query(const char* sql, boost::mysql::results &result) {
    try {
        _conn.execute(sql, result);
    }
    catch (const boost::mysql::error_with_diagnostics& err) {
        std::cerr << "Error: " << err.what() << '\n'
            << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        return false;
    }
    catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << std::endl;
        return false;
    }
    return true;
}

void DBConnection::createTables() {

    printf("Checking tables ... ");

    const int face_bytes = FACE_VEC_SIZE * 4;
    boost::mysql::results r;
    query(fmt::format("CREATE TABLE IF NOT EXISTS updates (\
        id INT AUTO_INCREMENT PRIMARY KEY, \
        device_id INT, \
        facial_features BLOB({}), \
        time TIMESTAMP DEFAULT CURRENT_TIMESTAMP \
    )", face_bytes).c_str(), r);
    // needs to include face vec variances
    // needs expected dts between devs for periods
    query(fmt::format("CREATE TABLE IF NOT EXISTS long_term_states (\
        id INT AUTO_INCREMENT PRIMARY KEY, \
        mean_facial_features BLOB({}) \
    )", face_bytes).c_str(), r);
    query(fmt::format("CREATE TABLE IF NOT EXISTS short_term_states (\
        id INT AUTO_INCREMENT PRIMARY KEY NOT NULL, \
        mean_facial_features BLOB(512) NOT NULL, \
        last_update_device_id INT NOT NULL, \
        last_update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
        expected_next_update_device_id INT, \
        expected_next_update_time TIMESTAMP NULL DEFAULT NULL, \
        expected_next_update_time_variance FLOAT(0), \
        long_term_state_key INT, \
        CONSTRAINT FK_lts FOREIGN KEY (long_term_state_key) REFERENCES long_term_states(id) \
    )", face_bytes).c_str(), r);

    printf("Done\n");

}

void DBConnection::getUpdates(std::vector<UpdatePtr> &updates) {
    //printf("Fetching updates ... ");
    boost::mysql::results result;
    query("SELECT id, device_id, facial_features FROM updates ORDER BY time ASC", result);
    if (!result.empty()) {
        for (const boost::mysql::row_view& row : result.rows()) {
            updates.push_back(UpdatePtr(new Update(row[0].as_int64(), row[1].as_int64(), row[2].as_blob())));
        }
    }
    //printf("Done\n");
}

void DBConnection::removeUpdate(int id) {
    try {
        fmt::print("Removing update {}\n", id);
        boost::mysql::results result;
        _conn.execute(_conn.prepare_statement("DELETE FROM updates WHERE id=?").bind(id), result);
        printf("Done\n");
    }
    catch (const boost::mysql::error_with_diagnostics& err) {
        std::cerr << "Error: " << err.what() << '\n'
            << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
    }
}

LongTermStatePtr DBConnection::getLongTermState(int id) {
    try {
        printf("Fetching long term state ... ");
        boost::mysql::results result;
        _conn.execute(_conn.prepare_statement(
            "SELECT id, mean_facial_features FROM long_term_states WHERE id=?"
        ).bind(id), result);
        printf("Done\n");
        auto row = result.rows()[0];
        return LongTermStatePtr(new LongTermState(row[0].as_int64(), row[1].as_blob()));
    }
    catch (const boost::mysql::error_with_diagnostics& err) {
        std::cerr << "Error: " << err.what() << '\n'
            << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
    }
}

void DBConnection::getLongTermStates(std::vector<EntityStatePtr> &states) {
    printf("Fetching long term states ... ");
    boost::mysql::results result;
    query("SELECT id, mean_facial_features FROM long_term_states ORDER BY id ASC", result);
    if (!result.empty()) {
        for (const boost::mysql::row_view& row : result.rows()) {
            states.push_back(EntityStatePtr(new LongTermState(row[0].as_int64(), row[1].as_blob())));
        }
    }
    printf("Done\n");
}

void DBConnection::createLongTermState(ShortTermStatePtr sts) {
    try {
        printf("Creating long term state ... ");
        boost::mysql::results result;
        _conn.execute(_conn.prepare_statement(
            "INSERT INTO long_term_states (mean_facial_features) VALUES(?)"
        ).bind(sts->getFacialFeatures()), result);
        printf("Done\n");
    }
    catch (const boost::mysql::error_with_diagnostics& err) {
        std::cerr << "Error: " << err.what() << '\n'
            << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
    }
}

void DBConnection::getShortTermStates(std::vector<EntityStatePtr> &states) {
    printf("Fetching short term states ... ");
    boost::mysql::results result;
    query("SELECT id, mean_facial_features, last_update_device_id, long_term_state_key FROM short_term_states ORDER BY id ASC", result);
    if (!result.empty()) {
        for (const boost::mysql::row_view& row : result.rows()) {
            if (row[3].is_int64()) {
                states.push_back(EntityStatePtr(new ShortTermState(row[0].as_int64(), row[1].as_blob(), row[2].as_int64(), row[3].as_int64())));
            } else {
                states.push_back(EntityStatePtr(new ShortTermState(row[0].as_int64(), row[1].as_blob(), row[2].as_int64())));
            }
        }
    }
    printf("Done\n");
}

void DBConnection::createShortTermState(UpdateCPtr update, LongTermStatePtr ltState) {
    try {
        printf("Creating short term state ... ");
        boost::mysql::results result;
        if (ltState != nullptr) {
            _conn.execute(_conn.prepare_statement(
                "INSERT INTO short_term_states (mean_facial_features, last_update_device_id, long_term_state_key) VALUES(?,?,?)"
            ).bind(update->getFacialFeatures(), update->deviceId, ltState->id), result);
        } else {
            _conn.execute(_conn.prepare_statement(
                "INSERT INTO short_term_states (mean_facial_features, last_update_device_id) VALUES(?,?)"
            ).bind(update->getFacialFeatures(), update->deviceId), result);
        }
        printf("Done\n");
    }
    catch (const boost::mysql::error_with_diagnostics& err) {
        std::cerr << "Error: " << err.what() << '\n'
            << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
    }
}

void DBConnection::updateShortTermState(ShortTermStatePtr state) {
    try {
        printf("Updating short term state ... ");
        boost::mysql::results result;
        if (state->longTermStateKey != -1) {
            _conn.execute(_conn.prepare_statement(
                "UPDATE short_term_states SET mean_facial_features=?, last_update_device_id=?, long_term_state_key=? WHERE id=?"
            ).bind(state->getFacialFeatures(), state->lastUpdateDeviceId, state->longTermStateKey, state->id), result);
        } else {
            _conn.execute(_conn.prepare_statement(
                "UPDATE short_term_states SET mean_facial_features=?, last_update_device_id=? WHERE id=?"
            ).bind(state->getFacialFeatures(), state->lastUpdateDeviceId, state->id), result);
        }
        printf("Done\n");
    }
    catch (const boost::mysql::error_with_diagnostics& err) {
        std::cerr << "Error: " << err.what() << '\n'
            << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
    }
}

void DBConnection::clearShortTermStates() {
    printf("Clearing short term states ... ");
    boost::mysql::results result;
    query("DELETE FROM short_term_states", result);
    printf("Done\n");

}