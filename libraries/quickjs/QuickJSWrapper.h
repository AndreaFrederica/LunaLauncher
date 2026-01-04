// SPDX-License-Identifier: GPL-3.0-only
// QuickJS C++ Wrapper for Luna Launcher

#pragma once

#include <memory>
#include <string>
#include <functional>
#include <stdexcept>
#include <vector>

extern "C" {
#include <quickjs.h>
#include <quickjs-libc.h>
}

namespace QuickJS {

class JSException : public std::runtime_error {
public:
    explicit JSException(const std::string& message) : std::runtime_error(message) {}
};

class JSValue;
class JSContext;

/**
 * @brief RAII wrapper for QuickJS runtime
 */
class JSRuntime {
public:
    JSRuntime() {
        m_runtime = JS_NewRuntime();
        if (!m_runtime) {
            throw JSException("Failed to create JS runtime");
        }
    }

    ~JSRuntime() {
        if (m_runtime) {
            JS_FreeRuntime(m_runtime);
        }
    }

    // Disable copy
    JSRuntime(const JSRuntime&) = delete;
    JSRuntime& operator=(const JSRuntime&) = delete;

    // Enable move
    JSRuntime(JSRuntime&& other) noexcept : m_runtime(other.m_runtime) {
        other.m_runtime = nullptr;
    }

    JSRuntime& operator=(JSRuntime&& other) noexcept {
        if (this != &other) {
            if (m_runtime) {
                JS_FreeRuntime(m_runtime);
            }
            m_runtime = other.m_runtime;
            other.m_runtime = nullptr;
        }
        return *this;
    }

    ::JSRuntime* get() const { return m_runtime; }

    void setMemoryLimit(size_t limit) {
        JS_SetMemoryLimit(m_runtime, limit);
    }

    void setGCThreshold(size_t threshold) {
        JS_SetGCThreshold(m_runtime, threshold);
    }

private:
    ::JSRuntime* m_runtime = nullptr;
};

/**
 * @brief RAII wrapper for QuickJS context
 */
class JSContext {
public:
    explicit JSContext(JSRuntime& runtime) : m_runtime(runtime) {
        m_context = JS_NewContext(runtime.get());
        if (!m_context) {
            throw JSException("Failed to create JS context");
        }
    }

    ~JSContext() {
        if (m_context) {
            JS_FreeContext(m_context);
        }
    }

    // Disable copy
    JSContext(const JSContext&) = delete;
    JSContext& operator=(const JSContext&) = delete;

    // Enable move
    JSContext(JSContext&& other) noexcept
        : m_context(other.m_context), m_runtime(other.m_runtime) {
        other.m_context = nullptr;
    }

    JSContext& operator=(JSContext&& other) noexcept {
        if (this != &other) {
            if (m_context) {
                JS_FreeContext(m_context);
            }
            m_context = other.m_context;
            other.m_context = nullptr;
        }
        return *this;
    }

    ::JSContext* get() const { return m_context; }

    /**
     * @brief Evaluate JavaScript code
     * @param code JavaScript source code
     * @param filename Optional filename for error reporting
     * @return Result as string
     */
    std::string eval(const std::string& code, const std::string& filename = "<eval>") {
        ::JSValue val = JS_Eval(m_context, code.c_str(), code.length(),
                               filename.c_str(), JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(val)) {
            ::JSValue exception = JS_GetException(m_context);
            const char* str = JS_ToCString(m_context, exception);
            std::string error_msg = str ? str : "Unknown error";
            JS_FreeCString(m_context, str);
            JS_FreeValue(m_context, exception);
            JS_FreeValue(m_context, val);
            throw JSException(error_msg);
        }

        const char* str = JS_ToCString(m_context, val);
        std::string result = str ? str : "";
        JS_FreeCString(m_context, str);
        JS_FreeValue(m_context, val);

        return result;
    }

    /**
     * @brief Evaluate JavaScript code and return as integer
     */
    int evalAsInt(const std::string& code, const std::string& filename = "<eval>") {
        ::JSValue val = JS_Eval(m_context, code.c_str(), code.length(),
                               filename.c_str(), JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(val)) {
            ::JSValue exception = JS_GetException(m_context);
            const char* str = JS_ToCString(m_context, exception);
            std::string error_msg = str ? str : "Unknown error";
            JS_FreeCString(m_context, str);
            JS_FreeValue(m_context, exception);
            JS_FreeValue(m_context, val);
            throw JSException(error_msg);
        }

        int result = 0;
        JS_ToInt32(m_context, &result, val);
        JS_FreeValue(m_context, val);

        return result;
    }

    /**
     * @brief Evaluate JavaScript code and return as double
     */
    double evalAsDouble(const std::string& code, const std::string& filename = "<eval>") {
        ::JSValue val = JS_Eval(m_context, code.c_str(), code.length(),
                               filename.c_str(), JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(val)) {
            ::JSValue exception = JS_GetException(m_context);
            const char* str = JS_ToCString(m_context, exception);
            std::string error_msg = str ? str : "Unknown error";
            JS_FreeCString(m_context, str);
            JS_FreeValue(m_context, exception);
            JS_FreeValue(m_context, val);
            throw JSException(error_msg);
        }

        double result = 0.0;
        JS_ToFloat64(m_context, &result, val);
        JS_FreeValue(m_context, val);

        return result;
    }

    /**
     * @brief Evaluate JavaScript code and return as boolean
     */
    bool evalAsBool(const std::string& code, const std::string& filename = "<eval>") {
        ::JSValue val = JS_Eval(m_context, code.c_str(), code.length(),
                               filename.c_str(), JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(val)) {
            ::JSValue exception = JS_GetException(m_context);
            const char* str = JS_ToCString(m_context, exception);
            std::string error_msg = str ? str : "Unknown error";
            JS_FreeCString(m_context, str);
            JS_FreeValue(m_context, exception);
            JS_FreeValue(m_context, val);
            throw JSException(error_msg);
        }

        bool result = JS_ToBool(m_context, val);
        JS_FreeValue(m_context, val);

        return result;
    }

    /**
     * @brief Set a global variable
     */
    void setGlobal(const std::string& name, const std::string& value) {
        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue str_val = JS_NewString(m_context, value.c_str());
        JS_SetPropertyStr(m_context, global, name.c_str(), str_val);
        JS_FreeValue(m_context, global);
    }

    void setGlobal(const std::string& name, int value) {
        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue int_val = JS_NewInt32(m_context, value);
        JS_SetPropertyStr(m_context, global, name.c_str(), int_val);
        JS_FreeValue(m_context, global);
    }

    void setGlobal(const std::string& name, double value) {
        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue double_val = JS_NewFloat64(m_context, value);
        JS_SetPropertyStr(m_context, global, name.c_str(), double_val);
        JS_FreeValue(m_context, global);
    }

    void setGlobal(const std::string& name, bool value) {
        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue bool_val = JS_NewBool(m_context, value);
        JS_SetPropertyStr(m_context, global, name.c_str(), bool_val);
        JS_FreeValue(m_context, global);
    }

    /**
     * @brief Get a global variable as string
     */
    std::string getGlobalAsString(const std::string& name) {
        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue val = JS_GetPropertyStr(m_context, global, name.c_str());
        JS_FreeValue(m_context, global);

        const char* str = JS_ToCString(m_context, val);
        std::string result = str ? str : "";
        JS_FreeCString(m_context, str);
        JS_FreeValue(m_context, val);

        return result;
    }

    /**
     * @brief Call a JavaScript function
     */
    std::string callFunction(const std::string& funcName, const std::vector<std::string>& args = {}) {
        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue func = JS_GetPropertyStr(m_context, global, funcName.c_str());

        if (!JS_IsFunction(m_context, func)) {
            JS_FreeValue(m_context, func);
            JS_FreeValue(m_context, global);
            throw JSException("Not a function: " + funcName);
        }

        std::vector<::JSValue> js_args;
        for (const auto& arg : args) {
            js_args.push_back(JS_NewString(m_context, arg.c_str()));
        }

        ::JSValue result = JS_Call(m_context, func, global, js_args.size(),
                                   js_args.empty() ? nullptr : js_args.data());

        // Free arguments
        for (auto& arg : js_args) {
            JS_FreeValue(m_context, arg);
        }
        JS_FreeValue(m_context, func);
        JS_FreeValue(m_context, global);

        if (JS_IsException(result)) {
            ::JSValue exception = JS_GetException(m_context);
            const char* str = JS_ToCString(m_context, exception);
            std::string error_msg = str ? str : "Unknown error";
            JS_FreeCString(m_context, str);
            JS_FreeValue(m_context, exception);
            JS_FreeValue(m_context, result);
            throw JSException(error_msg);
        }

        const char* str = JS_ToCString(m_context, result);
        std::string ret = str ? str : "";
        JS_FreeCString(m_context, str);
        JS_FreeValue(m_context, result);

        return ret;
    }

    /**
     * @brief Load and execute JavaScript code from file
     * @param filepath Path to the JavaScript file
     * @return Result as string
     */
    std::string evalFile(const std::string& filepath) {
        // Read file content
        FILE* file = fopen(filepath.c_str(), "rb");
        if (!file) {
            throw JSException("Failed to open file: " + filepath);
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::vector<char> buffer(size + 1);
        size_t read = fread(buffer.data(), 1, size, file);
        fclose(file);
        buffer[read] = '\0';

        std::string code(buffer.data(), read);
        return eval(code, filepath);
    }

    /**
     * @brief Register a C++ function that returns string
     */
    using JSCFunctionString = std::function<std::string(const std::vector<std::string>&)>;

    void registerFunction(const std::string& name, JSCFunctionString func) {
        // Store the function in a map for later use
        auto* func_ptr = new JSCFunctionString(std::move(func));

        auto wrapper = [](::JSContext* ctx, ::JSValueConst this_val, int argc, ::JSValueConst* argv) -> ::JSValue {
            // Get the function pointer from opaque data
            auto* func = static_cast<JSCFunctionString*>(JS_GetContextOpaque(ctx));
            if (!func) {
                return JS_EXCEPTION;
            }

            // Convert arguments
            std::vector<std::string> args;
            for (int i = 0; i < argc; i++) {
                const char* str = JS_ToCString(ctx, argv[i]);
                if (str) {
                    args.emplace_back(str);
                    JS_FreeCString(ctx, str);
                }
            }

            try {
                // Call the C++ function
                std::string result = (*func)(args);
                return JS_NewString(ctx, result.c_str());
            } catch (const std::exception& e) {
                return JS_ThrowInternalError(ctx, "%s", e.what());
            }
        };

        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue func_obj = JS_NewCFunction(m_context, wrapper, name.c_str(), 0);
        JS_SetPropertyStr(m_context, global, name.c_str(), func_obj);
        JS_FreeValue(m_context, global);

        // Store function pointer in context opaque
        JS_SetContextOpaque(m_context, func_ptr);
    }

    /**
     * @brief Register a C++ function with simple signature (no args, returns string)
     */
    using JSCFunctionSimple = std::function<std::string()>;

    void registerSimpleFunction(const std::string& name, JSCFunctionSimple func) {
        struct FuncWrapper {
            JSCFunctionSimple func;
            ::JSContext* ctx;
        };

        auto* wrapper_data = new FuncWrapper{std::move(func), m_context};

        auto wrapper = [](::JSContext* ctx, ::JSValueConst, int, ::JSValueConst*) -> ::JSValue {
            auto* data = static_cast<FuncWrapper*>(JS_GetContextOpaque(ctx));
            if (!data) {
                return JS_EXCEPTION;
            }

            try {
                std::string result = data->func();
                return JS_NewString(ctx, result.c_str());
            } catch (const std::exception& e) {
                return JS_ThrowInternalError(ctx, "%s", e.what());
            }
        };

        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue func_obj = JS_NewCFunction(m_context, wrapper, name.c_str(), 0);
        JS_SetPropertyStr(m_context, global, name.c_str(), func_obj);
        JS_FreeValue(m_context, global);
    }

    /**
     * @brief Register C++ callback for console.log
     */
    using ConsoleLogCallback = std::function<void(const std::string&)>;

    void setConsoleLog(ConsoleLogCallback callback) {
        struct CallbackWrapper {
            ConsoleLogCallback callback;
        };

        auto* wrapper_data = new CallbackWrapper{std::move(callback)};

        auto wrapper = [](::JSContext* ctx, ::JSValueConst, int argc, ::JSValueConst* argv) -> ::JSValue {
            auto* data = static_cast<CallbackWrapper*>(JS_GetContextOpaque(ctx));
            if (data && argc > 0) {
                const char* str = JS_ToCString(ctx, argv[0]);
                if (str) {
                    data->callback(str);
                    JS_FreeCString(ctx, str);
                }
            }
            return JS_UNDEFINED;
        };

        eval(R"(
            var console = {
                log: function() {}
            };
        )");

        ::JSValue global = JS_GetGlobalObject(m_context);
        ::JSValue console_obj = JS_GetPropertyStr(m_context, global, "console");
        ::JSValue log_func = JS_NewCFunction(m_context, wrapper, "log", 1);
        JS_SetPropertyStr(m_context, console_obj, "log", log_func);
        JS_FreeValue(m_context, console_obj);
        JS_FreeValue(m_context, global);

        JS_SetContextOpaque(m_context, wrapper_data);
    }

    /**
     * @brief Enable standard library (file I/O, timers, etc)
     */
    void enableStdLib() {
        js_std_add_helpers(m_context, 0, nullptr);
        js_init_module_std(m_context, "std");
        js_init_module_os(m_context, "os");
    }

private:
    ::JSContext* m_context = nullptr;
    JSRuntime& m_runtime;
};

/**
 * @brief Convenience class combining runtime and context
 */
class JSEngine {
public:
    JSEngine() : m_runtime(), m_context(m_runtime) {}

    JSContext& context() { return m_context; }
    JSRuntime& runtime() { return m_runtime; }

    std::string eval(const std::string& code, const std::string& filename = "<eval>") {
        return m_context.eval(code, filename);
    }

    int evalAsInt(const std::string& code, const std::string& filename = "<eval>") {
        return m_context.evalAsInt(code, filename);
    }

    double evalAsDouble(const std::string& code, const std::string& filename = "<eval>") {
        return m_context.evalAsDouble(code, filename);
    }

    bool evalAsBool(const std::string& code, const std::string& filename = "<eval>") {
        return m_context.evalAsBool(code, filename);
    }

    void setGlobal(const std::string& name, const std::string& value) {
        m_context.setGlobal(name, value);
    }

    void setGlobal(const std::string& name, int value) {
        m_context.setGlobal(name, value);
    }

    void setGlobal(const std::string& name, double value) {
        m_context.setGlobal(name, value);
    }

    void setGlobal(const std::string& name, bool value) {
        m_context.setGlobal(name, value);
    }

    std::string getGlobalAsString(const std::string& name) {
        return m_context.getGlobalAsString(name);
    }

    std::string callFunction(const std::string& funcName, const std::vector<std::string>& args = {}) {
        return m_context.callFunction(funcName, args);
    }

    std::string evalFile(const std::string& filepath) {
        return m_context.evalFile(filepath);
    }

    void registerFunction(const std::string& name, JSContext::JSCFunctionString func) {
        m_context.registerFunction(name, std::move(func));
    }

    void registerSimpleFunction(const std::string& name, JSContext::JSCFunctionSimple func) {
        m_context.registerSimpleFunction(name, std::move(func));
    }

    void setConsoleLog(JSContext::ConsoleLogCallback callback) {
        m_context.setConsoleLog(std::move(callback));
    }

    void enableStdLib() {
        m_context.enableStdLib();
    }

private:
    JSRuntime m_runtime;
    JSContext m_context;
};

} // namespace QuickJS
