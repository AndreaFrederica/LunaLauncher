// SPDX-License-Identifier: GPL-3.0-only
// QuickJS C++ Wrapper Advanced Examples - JS ↔ C++ Interop

#include "QuickJSWrapper.h"
#include <iostream>
#include <fstream>

namespace QuickJS {
namespace AdvancedExamples {

/**
 * @brief Example: C++ functions callable from JavaScript
 */
void cppFunctionsInJS() {
    try {
        JSEngine engine;

        // Register a simple C++ function
        engine.registerSimpleFunction("getVersion", []() {
            return "Luna Launcher v1.0.0";
        });

        // Register a function with parameters
        engine.registerFunction("multiply", [](const std::vector<std::string>& args) {
            if (args.size() < 2) return std::string("Error: Need 2 arguments");
            double a = std::stod(args[0]);
            double b = std::stod(args[1]);
            return std::to_string(a * b);
        });

        // Register file reading function
        engine.registerFunction("readFile", [](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Error: Need filename");
            std::ifstream file(args[0]);
            if (!file) return std::string("Error: Cannot open file");
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            return content;
        });

        // Call C++ functions from JavaScript
        std::string result = engine.eval(R"(
            var version = getVersion();
            var product = multiply('5', '10');
            version + ' - Product: ' + product
        )");

        std::cout << "Result: " << result << std::endl;

    } catch (const JSException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

/**
 * @brief Example: Custom console.log implementation
 */
void customConsoleLog() {
    try {
        JSEngine engine;

        // Set custom console.log handler
        engine.setConsoleLog([](const std::string& message) {
            std::cout << "[JS Console] " << message << std::endl;
        });

        // JavaScript can now use console.log
        engine.eval(R"(
            console.log('Hello from JavaScript!');
            console.log('Computing: ' + (2 + 2));

            for (var i = 0; i < 3; i++) {
                console.log('Loop iteration: ' + i);
            }
        )");

    } catch (const JSException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

/**
 * @brief Example: Load and execute JS file
 */
void loadJSFile() {
    try {
        // Create a test JS file
        {
            std::ofstream file("test_script.js");
            file << R"(
// Test JavaScript file
var launcherName = 'Luna Launcher';
var features = ['Fast', 'Customizable', 'Open Source'];

function getInfo() {
    return launcherName + ' - Features: ' + features.join(', ');
}

function processData(data) {
    return 'Processed: ' + data.toUpperCase();
}

getInfo();
)";
        }

        JSEngine engine;

        // Load and execute the file
        std::string result = engine.evalFile("test_script.js");
        std::cout << "File result: " << result << std::endl;

        // Call functions defined in the file
        std::string processed = engine.callFunction("processData", {"hello world"});
        std::cout << "Processed: " << processed << std::endl;

    } catch (const JSException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

/**
 * @brief Example: Bi-directional data exchange
 */
void bidirectionalDataExchange() {
    try {
        JSEngine engine;

        // C++ provides data to JS
        engine.setGlobal("serverUrl", "https://api.example.com");
        engine.setGlobal("maxRetries", 3);
        engine.setGlobal("enableLogging", true);

        // Register C++ functions for JS to call
        engine.registerFunction("fetchData", [](const std::vector<std::string>& args) {
            // Simulate fetching data
            return "{\"status\":\"success\",\"data\":[1,2,3]}";
        });

        engine.registerFunction("logMessage", [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                std::cout << "[C++ Log] " << args[0] << std::endl;
            }
            return "logged";
        });

        // JS processes data and calls back to C++
        engine.eval(R"(
            logMessage('Starting data fetch from: ' + serverUrl);

            var rawData = fetchData();
            var parsed = JSON.parse(rawData);

            if (parsed.status === 'success') {
                logMessage('Data received: ' + parsed.data.length + ' items');

                // Process data in JavaScript
                var processed = parsed.data.map(function(x) {
                    return x * 2;
                });

                // Store result for C++ to read
                var result = {
                    processed: processed,
                    count: processed.length
                };
            }
        )");

        // C++ reads the result
        int count = engine.evalAsInt("result.count");
        std::string processed = engine.eval("JSON.stringify(result.processed)");

        std::cout << "Processed count: " << count << std::endl;
        std::cout << "Processed data: " << processed << std::endl;

    } catch (const JSException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

/**
 * @brief Example: Plugin system using JavaScript
 */
void pluginSystem() {
    try {
        JSEngine engine;

        // Set up console.log
        engine.setConsoleLog([](const std::string& msg) {
            std::cout << "[Plugin] " << msg << std::endl;
        });

        // Register plugin API
        engine.registerFunction("registerCommand", [](const std::vector<std::string>& args) {
            if (args.size() >= 2) {
                std::cout << "[C++] Registered command: " << args[0]
                         << " -> " << args[1] << std::endl;
            }
            return "ok";
        });

        engine.registerFunction("showNotification", [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                std::cout << "[Notification] " << args[0] << std::endl;
            }
            return "shown";
        });

        // Load a plugin script
        std::string pluginCode = R"(
// Plugin: Welcome Message
(function() {
    console.log('Loading Welcome Plugin...');

    // Register plugin commands
    registerCommand('welcome', 'Shows welcome message');
    registerCommand('info', 'Shows launcher info');

    // Define plugin functions
    function onEnable() {
        showNotification('Welcome Plugin Enabled!');
        console.log('Welcome plugin is now active');
    }

    function onCommand(cmd) {
        if (cmd === 'welcome') {
            showNotification('Welcome to Luna Launcher!');
            return true;
        } else if (cmd === 'info') {
            showNotification('Luna Launcher - Built with QuickJS');
            return true;
        }
        return false;
    }

    // Export plugin API
    var plugin = {
        name: 'WelcomePlugin',
        version: '1.0',
        onEnable: onEnable,
        onCommand: onCommand
    };

    return plugin;
})();
)";

        engine.eval(pluginCode);
        engine.eval("plugin.onEnable()");

        // Simulate command execution
        bool handled = engine.evalAsBool("plugin.onCommand('welcome')");
        std::cout << "Command handled: " << (handled ? "yes" : "no") << std::endl;

    } catch (const JSException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

/**
 * @brief Example: Configuration system with JS
 */
void configurationSystem() {
    try {
        // Create a config file
        {
            std::ofstream file("launcher_config.js");
            file << R"(
// Launcher Configuration
var config = {
    launcher: {
        name: 'Luna Launcher',
        version: '1.0.0',
        theme: 'dark'
    },

    network: {
        timeout: 30000,
        maxRetries: 3,
        mirrors: [
            'https://primary.example.com',
            'https://backup.example.com'
        ]
    },

    features: {
        autoUpdate: true,
        telemetry: false,
        beta: false
    },

    // Helper function to get mirror by index
    getMirror: function(index) {
        if (index < this.network.mirrors.length) {
            return this.network.mirrors[index];
        }
        return this.network.mirrors[0];
    },

    // Validate configuration
    validate: function() {
        if (!this.launcher.name) return 'Missing launcher name';
        if (this.network.timeout < 1000) return 'Timeout too short';
        if (this.network.mirrors.length === 0) return 'No mirrors configured';
        return 'valid';
    }
};

config;
)";
        }

        JSEngine engine;

        // Load configuration
        engine.evalFile("launcher_config.js");

        // Register C++ function to apply config
        engine.registerFunction("applyConfig", [](const std::vector<std::string>& args) {
            std::cout << "[C++] Applying configuration: " << args[0] << std::endl;
            return "applied";
        });

        // Validate and use configuration
        std::string validation = engine.eval("config.validate()");
        std::cout << "Config validation: " << validation << std::endl;

        if (validation == "valid") {
            std::string name = engine.eval("config.launcher.name");
            std::string version = engine.eval("config.launcher.version");
            int timeout = engine.evalAsInt("config.network.timeout");
            bool autoUpdate = engine.evalAsBool("config.features.autoUpdate");

            std::cout << "Name: " << name << std::endl;
            std::cout << "Version: " << version << std::endl;
            std::cout << "Timeout: " << timeout << "ms" << std::endl;
            std::cout << "Auto Update: " << (autoUpdate ? "enabled" : "disabled") << std::endl;

            // Get mirrors
            std::string mirror1 = engine.eval("config.getMirror(0)");
            std::cout << "Primary Mirror: " << mirror1 << std::endl;
        }

    } catch (const JSException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

/**
 * @brief Example: Async-like pattern with callbacks
 */
void asyncPattern() {
    try {
        JSEngine engine;

        // Register C++ async operation (simulated)
        engine.registerFunction("downloadFile", [](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("error");

            // Simulate download
            std::cout << "[C++] Downloading: " << args[0] << std::endl;

            // Return success
            return std::string("downloaded");
        });

        // Set up callback system
        engine.eval(R"(
            var callbacks = {};

            function onDownloadComplete(status) {
                if (callbacks.onComplete) {
                    callbacks.onComplete(status);
                }
            }

            function setCallback(name, func) {
                callbacks[name] = func;
            }

            // User code
            setCallback('onComplete', function(status) {
                console.log('Download finished with status: ' + status);
            });
        )");

        engine.setConsoleLog([](const std::string& msg) {
            std::cout << "[JS] " << msg << std::endl;
        });

        // Trigger download
        std::string result = engine.callFunction("downloadFile",
            {"https://example.com/file.zip"});

        // Trigger callback
        engine.callFunction("onDownloadComplete", {result});

    } catch (const JSException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

} // namespace AdvancedExamples
} // namespace QuickJS

/*
// Real-world usage example in Luna Launcher:

#include "QuickJSWrapper.h"

class ScriptingManager {
public:
    ScriptingManager() {
        // Set up JS engine with C++ bindings
        m_engine.setConsoleLog([](const std::string& msg) {
            qDebug() << "[Script]" << QString::fromStdString(msg);
        });

        // Register launcher API
        m_engine.registerFunction("getLauncherVersion", []() {
            return BuildConfig.VERSION_STRING.toStdString();
        });

        m_engine.registerFunction("getMinecraftPath", []() {
            return APPLICATION->minecraftRoot().toStdString();
        });

        m_engine.registerSimpleFunction("getUsername", []() {
            return APPLICATION->accounts()->activeAccount()->username().toStdString();
        });
    }

    bool loadPlugin(const QString& scriptPath) {
        try {
            m_engine.evalFile(scriptPath.toStdString());
            return true;
        } catch (const QuickJS::JSException& e) {
            qWarning() << "Failed to load plugin:" << e.what();
            return false;
        }
    }

private:
    QuickJS::JSEngine m_engine;
};
*/
