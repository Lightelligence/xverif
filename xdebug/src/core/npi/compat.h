#pragma once

#include "npi_fsdb.h"
#include "core/common/unique_resource.h"

#include <fstream>
#include <string>
#include <vector>

// Keep std::string/std::vector on the application's side of the ABI boundary.
// Some NPI L1 libraries use a different libstdc++ ABI for their C++ helpers.
// These adapters only pass C-compatible values to the public NPI primitives.
namespace xdebug_core {

struct CompatVctReleaser {
    void operator()(npiFsdbVctHandle vct) const {
        npi_fsdb_release_vct(vct);
    }
};

inline bool npi_sig_hdl_value_at_compat(npiFsdbSigHandle signal,
                                        npiFsdbTime time,
                                        std::string& out,
                                        npiFsdbValType format) {
    out.clear();
    if (!signal) return false;
    if (format != npiFsdbBinStrVal && format != npiFsdbHexStrVal &&
        format != npiFsdbDecStrVal) return false;

    UniqueResource<npiFsdbVctHandle, CompatVctReleaser> vct(
        npi_fsdb_create_vct(signal));
    if (!vct) return false;

    bool ok = npi_fsdb_goto_time(vct.get(), time) != 0;
    npiFsdbValue value = {};
    value.format = format;
    if (ok && npi_fsdb_vct_value(vct.get(), &value) != 0 && value.value.str) {
        out = value.value.str;
    } else {
        ok = false;
    }
    return ok;
}

template <typename HandleVec, typename ValueVec>
inline bool npi_sig_hdl_vec_value_at_compat(const HandleVec& signals,
                                            npiFsdbTime time,
                                            ValueVec& out,
                                            npiFsdbValType format) {
    out.clear();
    out.reserve(signals.size());
    for (typename HandleVec::const_iterator it = signals.begin();
         it != signals.end(); ++it) {
        std::string value;
        if (!npi_sig_hdl_value_at_compat(*it, time, value, format)) {
            out.clear();
            return false;
        }
        out.push_back(value);
    }
    return !signals.empty();
}

inline bool npi_sig_value_at_compat(npiFsdbFileHandle file,
                                    const char* signal_path,
                                    npiFsdbTime time,
                                    std::string& out,
                                    npiFsdbValType format) {
    if (!file || !signal_path) {
        out.clear();
        return false;
    }
    npiFsdbSigHandle signal =
        npi_fsdb_sig_by_name(file, signal_path, nullptr);
    return npi_sig_hdl_value_at_compat(signal, time, out, format);
}

inline bool npi_sig_vec_value_at_compat(
    npiFsdbFileHandle file,
    const std::vector<std::string>& signal_paths,
    npiFsdbTime time,
    std::vector<std::string>& out,
    npiFsdbValType format) {
    out.clear();
    std::vector<npiFsdbSigHandle> signals;
    signals.reserve(signal_paths.size());
    for (std::vector<std::string>::const_iterator it = signal_paths.begin();
         it != signal_paths.end(); ++it) {
        npiFsdbSigHandle signal =
            file ? npi_fsdb_sig_by_name(file, it->c_str(), nullptr) : nullptr;
        if (!signal) return false;
        signals.push_back(signal);
    }
    return npi_sig_hdl_vec_value_at_compat(signals, time, out, format);
}

inline bool read_source_file_line(const std::string& file,
                                  int line_number,
                                  std::string& out) {
    out.clear();
    if (file.empty() || line_number <= 0) return false;

    std::ifstream input(file.c_str());
    if (!input) return false;
    for (int current = 1; current <= line_number; ++current) {
        if (!std::getline(input, out)) {
            out.clear();
            return false;
        }
    }
    return true;
}

}  // namespace xdebug_core
