// Compile this standalone test with tests/unit/npi_stub before any Verdi/NPI
// include path.  The production compatibility header is otherwise unchanged.
#include "npi_fsdb.h"
#include "core/npi/compat.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

struct xdebug_test_npi_fsdb_sig {
    std::map<npiFsdbTime, std::string> values;
    bool create_fail;
    bool seek_fail;
    bool read_fail;
    bool null_text;

    xdebug_test_npi_fsdb_sig()
        : create_fail(false), seek_fail(false), read_fail(false), null_text(false) {}
};

struct xdebug_test_npi_fsdb_file {
    std::map<std::string, npiFsdbSigHandle> signals;
};

struct xdebug_test_npi_fsdb_vct {
    npiFsdbSigHandle signal;
    npiFsdbTime time;
};

namespace {

const npiFsdbValType kFourStateText = npiFsdbBinStrVal;
const npiFsdbValType kWideText = npiFsdbHexStrVal;
const npiFsdbValType kNonStringFormat = npiFsdbNonStringVal;

struct FakeNpiState {
    int create_calls;
    int seek_calls;
    int read_calls;
    int release_calls;
    int lookup_calls;
    npiFsdbValType last_format;
    std::vector<std::string> lookups;
    std::set<npiFsdbVctHandle> active_vcts;
    bool invalid_release;

    FakeNpiState()
        : create_calls(0),
          seek_calls(0),
          read_calls(0),
          release_calls(0),
          lookup_calls(0),
          last_format(npiFsdbBinStrVal),
          invalid_release(false) {}
};

FakeNpiState g_fake;

void reset_fake() {
    assert(g_fake.active_vcts.empty());
    g_fake = FakeNpiState();
}

void assert_no_active_vcts() {
    assert(g_fake.active_vcts.empty());
    assert(!g_fake.invalid_release);
}

}  // namespace

extern "C" npiFsdbVctHandle npi_fsdb_create_vct(npiFsdbSigHandle signal) {
    ++g_fake.create_calls;
    if (!signal || signal->create_fail) return nullptr;

    xdebug_test_npi_fsdb_vct* vct = new xdebug_test_npi_fsdb_vct;
    vct->signal = signal;
    vct->time = 0;
    g_fake.active_vcts.insert(vct);
    return vct;
}

extern "C" int npi_fsdb_goto_time(npiFsdbVctHandle vct, npiFsdbTime time) {
    ++g_fake.seek_calls;
    if (!vct || !vct->signal || vct->signal->seek_fail) return 0;
    vct->time = time;
    return 1;
}

extern "C" int npi_fsdb_vct_value(npiFsdbVctHandle vct,
                                     npiFsdbValue* value) {
    ++g_fake.read_calls;
    if (!vct || !vct->signal || !value || vct->signal->read_fail) return 0;

    g_fake.last_format = value->format;
    if (vct->signal->null_text) {
        value->value.str = nullptr;
        return 1;
    }

    std::map<npiFsdbTime, std::string>::const_iterator found =
        vct->signal->values.find(vct->time);
    if (found == vct->signal->values.end()) return 0;

    value->value.str = found->second.c_str();
    return 1;
}

extern "C" void npi_fsdb_release_vct(npiFsdbVctHandle vct) {
    ++g_fake.release_calls;
    if (!vct || g_fake.active_vcts.erase(vct) != 1) {
        g_fake.invalid_release = true;
        return;
    }
    delete vct;
}

extern "C" npiFsdbSigHandle npi_fsdb_sig_by_name(npiFsdbFileHandle file,
                                                   const char* path,
                                                   void* /*scope*/) {
    ++g_fake.lookup_calls;
    if (!file || !path) return nullptr;

    g_fake.lookups.push_back(path);
    std::map<std::string, npiFsdbSigHandle>::const_iterator found =
        file->signals.find(path);
    return found == file->signals.end() ? nullptr : found->second;
}

namespace {

void test_scalar_success_and_failures() {
    reset_fake();

    xdebug_test_npi_fsdb_sig signal;
    signal.values[10] = "1'bx";
    signal.values[20] =
        "128'h0123456789abcdef0123456789abcdef";

    std::string value = "stale";
    assert(xdebug_core::npi_sig_hdl_value_at_compat(
        &signal, 10, value, kFourStateText));
    assert(value == "1'bx");
    assert(g_fake.last_format == kFourStateText);
    assert(g_fake.create_calls == 1);
    assert(g_fake.seek_calls == 1);
    assert(g_fake.read_calls == 1);
    assert(g_fake.release_calls == 1);
    assert_no_active_vcts();

    value = "stale";
    assert(xdebug_core::npi_sig_hdl_value_at_compat(
        &signal, 20, value, kWideText));
    assert(value == "128'h0123456789abcdef0123456789abcdef");
    assert(g_fake.last_format == kWideText);
    assert(g_fake.release_calls == 2);
    assert_no_active_vcts();

    value = "stale";
    assert(!xdebug_core::npi_sig_hdl_value_at_compat(
        nullptr, 10, value, kFourStateText));
    assert(value.empty());
    assert(g_fake.create_calls == 2);
    assert_no_active_vcts();

    value = "stale";
    assert(!xdebug_core::npi_sig_hdl_value_at_compat(
        &signal, 10, value, kNonStringFormat));
    assert(value.empty());
    assert(g_fake.create_calls == 2);
    assert(g_fake.release_calls == 2);
    assert_no_active_vcts();

    signal.create_fail = true;
    value = "stale";
    assert(!xdebug_core::npi_sig_hdl_value_at_compat(
        &signal, 10, value, kFourStateText));
    assert(value.empty());
    assert(g_fake.create_calls == 3);
    assert(g_fake.release_calls == 2);
    assert_no_active_vcts();

    signal.create_fail = false;
    signal.seek_fail = true;
    value = "stale";
    assert(!xdebug_core::npi_sig_hdl_value_at_compat(
        &signal, 10, value, kFourStateText));
    assert(value.empty());
    assert(g_fake.seek_calls == 3);
    assert(g_fake.read_calls == 2);
    assert(g_fake.release_calls == 3);
    assert_no_active_vcts();

    signal.seek_fail = false;
    signal.read_fail = true;
    value = "stale";
    assert(!xdebug_core::npi_sig_hdl_value_at_compat(
        &signal, 10, value, kFourStateText));
    assert(value.empty());
    assert(g_fake.seek_calls == 4);
    assert(g_fake.read_calls == 3);
    assert(g_fake.release_calls == 4);
    assert_no_active_vcts();

    signal.read_fail = false;
    signal.null_text = true;
    value = "stale";
    assert(!xdebug_core::npi_sig_hdl_value_at_compat(
        &signal, 10, value, kFourStateText));
    assert(value.empty());
    assert(g_fake.seek_calls == 5);
    assert(g_fake.read_calls == 4);
    assert(g_fake.release_calls == 5);
    assert_no_active_vcts();
}

void test_handle_vector_order_and_transactionality() {
    reset_fake();

    xdebug_test_npi_fsdb_sig first;
    first.values[7] = "first";
    xdebug_test_npi_fsdb_sig second;
    second.values[7] = "second";

    std::vector<npiFsdbSigHandle> handles;
    handles.push_back(&first);
    handles.push_back(&second);

    std::vector<std::string> values;
    values.push_back("stale");
    assert(xdebug_core::npi_sig_hdl_vec_value_at_compat(
        handles, 7, values, kFourStateText));
    assert(values.size() == 2);
    assert(values[0] == "first");
    assert(values[1] == "second");
    assert(g_fake.create_calls == 2);
    assert(g_fake.seek_calls == 2);
    assert(g_fake.read_calls == 2);
    assert(g_fake.release_calls == 2);
    assert_no_active_vcts();

    values.push_back("stale");
    std::vector<npiFsdbSigHandle> empty_handles;
    assert(!xdebug_core::npi_sig_hdl_vec_value_at_compat(
        empty_handles, 7, values, kFourStateText));
    assert(values.empty());
    assert_no_active_vcts();

    second.seek_fail = true;
    values.push_back("old result");
    assert(!xdebug_core::npi_sig_hdl_vec_value_at_compat(
        handles, 7, values, kFourStateText));
    assert(values.empty());
    assert(g_fake.create_calls == 4);
    assert(g_fake.seek_calls == 4);
    assert(g_fake.read_calls == 3);
    assert(g_fake.release_calls == 4);
    assert_no_active_vcts();
}

void test_named_lookups_and_vector_order() {
    reset_fake();

    xdebug_test_npi_fsdb_sig first;
    first.values[3] = "first-by-name";
    xdebug_test_npi_fsdb_sig second;
    second.values[3] = "second-by-name";
    xdebug_test_npi_fsdb_file file;
    file.signals["first"] = &first;
    file.signals["second"] = &second;

    std::string value = "stale";
    assert(xdebug_core::npi_sig_value_at_compat(
        &file, "first", 3, value, kFourStateText));
    assert(value == "first-by-name");
    assert(g_fake.lookups.size() == 1);
    assert(g_fake.lookups[0] == "first");
    assert_no_active_vcts();

    value = "stale";
    assert(!xdebug_core::npi_sig_value_at_compat(
        &file, "missing", 3, value, kFourStateText));
    assert(value.empty());
    assert(g_fake.lookups.size() == 2);
    assert(g_fake.lookups[1] == "missing");
    assert_no_active_vcts();

    value = "stale";
    assert(!xdebug_core::npi_sig_value_at_compat(
        nullptr, "first", 3, value, kFourStateText));
    assert(value.empty());
    value = "stale";
    assert(!xdebug_core::npi_sig_value_at_compat(
        &file, nullptr, 3, value, kFourStateText));
    assert(value.empty());
    assert(g_fake.lookup_calls == 2);

    std::vector<std::string> paths;
    paths.push_back("second");
    paths.push_back("first");
    std::vector<std::string> values;
    values.push_back("stale");
    assert(xdebug_core::npi_sig_vec_value_at_compat(
        &file, paths, 3, values, kWideText));
    assert(values.size() == 2);
    assert(values[0] == "second-by-name");
    assert(values[1] == "first-by-name");
    assert(g_fake.lookups.size() == 4);
    assert(g_fake.lookups[2] == "second");
    assert(g_fake.lookups[3] == "first");
    assert(g_fake.last_format == kWideText);
    assert_no_active_vcts();

    paths.clear();
    paths.push_back("first");
    paths.push_back("missing");
    paths.push_back("second");
    values.clear();
    values.push_back("old result");
    assert(!xdebug_core::npi_sig_vec_value_at_compat(
        &file, paths, 3, values, kFourStateText));
    assert(values.empty());
    assert(g_fake.lookups.size() == 6);
    assert(g_fake.lookups[4] == "first");
    assert(g_fake.lookups[5] == "missing");
    assert(g_fake.create_calls == 3);
    assert(g_fake.release_calls == 3);
    assert_no_active_vcts();

    values.push_back("old result");
    paths.clear();
    assert(!xdebug_core::npi_sig_vec_value_at_compat(
        &file, paths, 3, values, kFourStateText));
    assert(values.empty());

    paths.push_back("first");
    values.push_back("old result");
    assert(!xdebug_core::npi_sig_vec_value_at_compat(
        nullptr, paths, 3, values, kFourStateText));
    assert(values.empty());
    assert_no_active_vcts();
}

std::string unique_test_path(const char* suffix) {
    const long long ticks = static_cast<long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const char* temporary_dir = std::getenv("XVERIF_TEST_TMPDIR");
    const std::string base = temporary_dir && *temporary_dir ? temporary_dir : ".";
    return base + "/npi_compat_test_" + std::to_string(ticks) + suffix;
}

void test_source_file_lines() {
    const std::string source_path = unique_test_path(".sv");
    {
        std::ofstream source(source_path.c_str(), std::ios::binary);
        assert(source.good());
        source << "line one\n\nline three";
        assert(source.good());
    }

    std::string line = "stale";
    assert(xdebug_core::read_source_file_line(source_path, 1, line));
    assert(line == "line one");
    assert(xdebug_core::read_source_file_line(source_path, 2, line));
    assert(line.empty());
    assert(xdebug_core::read_source_file_line(source_path, 3, line));
    assert(line == "line three");

    line = "stale";
    assert(!xdebug_core::read_source_file_line(source_path, 0, line));
    assert(line.empty());
    line = "stale";
    assert(!xdebug_core::read_source_file_line(source_path, -1, line));
    assert(line.empty());
    line = "stale";
    assert(!xdebug_core::read_source_file_line("", 1, line));
    assert(line.empty());

    line = "stale";
    assert(!xdebug_core::read_source_file_line(source_path, 4, line));
    assert(line.empty());

    const std::string missing_path = unique_test_path(".missing");
    line = "stale";
    assert(!xdebug_core::read_source_file_line(missing_path, 1, line));
    assert(line.empty());

    assert(std::remove(source_path.c_str()) == 0);
}

}  // namespace

int main() {
    test_scalar_success_and_failures();
    test_handle_vector_order_and_transactionality();
    test_named_lookups_and_vector_order();
    test_source_file_lines();

    reset_fake();
    xdebug_test_npi_fsdb_sig signal;
    signal.values[1] = "10xz";
    signal.values[2] = "18446744073709551616";
    std::string value;
    assert(xdebug_core::npi_sig_hdl_value_at_compat(&signal, 1, value, npiFsdbBinStrVal));
    assert(value == "10xz");
    assert(xdebug_core::npi_sig_hdl_value_at_compat(&signal, 2, value, npiFsdbDecStrVal));
    assert(value == "18446744073709551616");
    assert_no_active_vcts();

    std::cout << "test_npi_compat: PASS\n";
    return 0;
}
