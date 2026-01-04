// SPDX-License-Identifier: GPL-3.0-only
// QuickJS C++ Wrapper Usage Examples

#include "QuickJSWrapper.h"
#include <iostream>

namespace QuickJS {
namespace Examples {

/**
 * @brief Basic usage example
 */
void basicUsage() {
    try {
        JSEngine engine;

        // Simple evaluation
        std::string result = engine.eval("'Hello, ' + 'World!'");
        std::cout << "Result: " << result << std::endl;

        // Math operations
        int sum = engine.evalAsInt("1 + 2 + 3");
        std::cout << "Sum: " << sum << std::endl;

        double pi = engine.evalAsDouble("Math.PI");
        std::cout << "PI: " << pi << std::endl;

        bool isTrue = engine.evalAsBool("true && true");
        std::cout << "Boolean: " << isTrue << std::endl;

    } catch (const JSException& e) {
        std::cerr << "JavaScript error: " << e.what() << std::endl;
    }
}

/**
 * @brief Global variables example
 */
void globalVariables() {
    try {
        JSEngine engine;

        // Set global variables
        engine.setGlobal("appName", "Luna Launcher");
        engine.setGlobal("version", 1.0);
        engine.setGlobal("isDebug", true);

        // Use them in JavaScript
        std::string result = engine.eval("appName + ' v' + version");
        std::cout << "Result: " << result << std::endl;

        // Read global variable
        std::string name = engine.getGlobalAsString("appName");
        std::cout << "App name: " << name << std::endl;

    } catch (const JSException& e) {
        std::cerr << "JavaScript error: " << e.what() << std::endl;
    }
}

/**
 * @brief Function calls example
 */
void functionCalls() {
    try {
        JSEngine engine;

        // Define a function
        engine.eval(R"(
            function greet(name) {
                return 'Hello, ' + name + '!';
            }

            function add(a, b) {
                return Number(a) + Number(b);
            }
        )");

        // Call the function
        std::string greeting = engine.callFunction("greet", {"World"});
        std::cout << "Greeting: " << greeting << std::endl;

    } catch (const JSException& e) {
        std::cerr << "JavaScript error: " << e.what() << std::endl;
    }
}

/**
 * @brief Complex script example
 */
void complexScript() {
    try {
        JSEngine engine;

        // Execute a complex script
        engine.eval(R"(
            // Define a configuration object
            var config = {
                servers: [
                    { name: "Official", url: "https://minecraft.net" },
                    { name: "Mirror", url: "https://mirror.example.com" }
                ],
                settings: {
                    language: "en_US",
                    theme: "dark"
                }
            };

            // Define helper functions
            function getServerUrl(index) {
                if (index < 0 || index >= config.servers.length) {
                    return null;
                }
                return config.servers[index].url;
            }

            function getServerCount() {
                return config.servers.length;
            }
        )");

        // Use the defined functions
        int serverCount = engine.evalAsInt("getServerCount()");
        std::cout << "Server count: " << serverCount << std::endl;

        std::string serverUrl = engine.eval("getServerUrl(0)");
        std::cout << "First server URL: " << serverUrl << std::endl;

    } catch (const JSException& e) {
        std::cerr << "JavaScript error: " << e.what() << std::endl;
    }
}

/**
 * @brief Error handling example
 */
void errorHandling() {
    try {
        JSEngine engine;

        // This will throw an exception
        engine.eval("undefined_variable");

    } catch (const JSException& e) {
        std::cout << "Caught expected error: " << e.what() << std::endl;
    }

    try {
        JSEngine engine;

        // Syntax error
        engine.eval("function {");

    } catch (const JSException& e) {
        std::cout << "Caught syntax error: " << e.what() << std::endl;
    }
}

/**
 * @brief JSON processing example
 */
void jsonProcessing() {
    try {
        JSEngine engine;

        // Parse JSON
        engine.eval(R"(
            var jsonStr = '{"name":"Steve","level":42,"items":["sword","pickaxe"]}';
            var player = JSON.parse(jsonStr);
        )");

        std::string playerName = engine.eval("player.name");
        int level = engine.evalAsInt("player.level");
        std::string items = engine.eval("player.items.join(', ')");

        std::cout << "Player: " << playerName << std::endl;
        std::cout << "Level: " << level << std::endl;
        std::cout << "Items: " << items << std::endl;

        // Create JSON
        std::string json = engine.eval(R"(
            JSON.stringify({
                username: "Alex",
                inventory: ["apple", "bread", "water"],
                health: 20
            })
        )");
        std::cout << "JSON: " << json << std::endl;

    } catch (const JSException& e) {
        std::cerr << "JavaScript error: " << e.what() << std::endl;
    }
}

} // namespace Examples
} // namespace QuickJS

/*
// Example usage in your application:

#include "QuickJSWrapper.h"

void myFunction() {
    try {
        QuickJS::JSEngine engine;

        // Simple calculation
        int result = engine.evalAsInt("2 + 2");
        qDebug() << "2 + 2 =" << result;

        // String manipulation
        engine.setGlobal("playerName", "Steve");
        std::string greeting = engine.eval("'Welcome, ' + playerName + '!'");
        qDebug() << QString::fromStdString(greeting);

    } catch (const QuickJS::JSException& e) {
        qWarning() << "JavaScript error:" << e.what();
    }
}
*/
