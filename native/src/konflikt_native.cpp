#include "konflikt_native.hpp"
#include <chrono>

namespace konflikt {

// Helper to get current timestamp in milliseconds
uint64_t GetTimestamp() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

// Helper to create State object
Napi::Object StateToObject(Napi::Env env, const State& state) {
    auto obj = Napi::Object::New(env);
    obj.Set("keyboardModifiers", Napi::Number::New(env, state.keyboard_modifiers));
    obj.Set("mouseButtons", Napi::Number::New(env, state.mouse_buttons));
    obj.Set("x", Napi::Number::New(env, state.x));
    obj.Set("y", Napi::Number::New(env, state.y));
    return obj;
}

// Helper to create Desktop object
Napi::Object DesktopToObject(Napi::Env env, const Desktop& desktop) {
    auto obj = Napi::Object::New(env);
    obj.Set("width", Napi::Number::New(env, desktop.width));
    obj.Set("height", Napi::Number::New(env, desktop.height));
    return obj;
}

// Helper to create Event object
Napi::Object EventToObject(Napi::Env env, const Event& event) {
    auto obj = Napi::Object::New(env);

    // Set type
    std::string type_str;
    switch (event.type) {
        case EventType::MouseMove: type_str = "mouseMove"; break;
        case EventType::MousePress: type_str = "mousePress"; break;
        case EventType::MouseRelease: type_str = "mouseRelease"; break;
        case EventType::KeyPress: type_str = "keyPress"; break;
        case EventType::KeyRelease: type_str = "keyRelease"; break;
        case EventType::DesktopChanged: type_str = "desktopChanged"; break;
    }
    obj.Set("type", Napi::String::New(env, type_str));

    // Set timestamp
    obj.Set("timestamp", Napi::Number::New(env, static_cast<double>(event.timestamp)));

    // Set state
    obj.Set("keyboardModifiers", Napi::Number::New(env, event.state.keyboard_modifiers));
    obj.Set("mouseButtons", Napi::Number::New(env, event.state.mouse_buttons));
    obj.Set("x", Napi::Number::New(env, event.state.x));
    obj.Set("y", Napi::Number::New(env, event.state.y));

    // Set button for mouse button events
    if (event.type == EventType::MousePress || event.type == EventType::MouseRelease) {
        obj.Set("button", Napi::Number::New(env, ToUInt32(event.button)));
    }

    // Set keycode and text for key events
    if (event.type == EventType::KeyPress || event.type == EventType::KeyRelease) {
        obj.Set("keycode", Napi::Number::New(env, event.keycode));
        if (!event.text.empty()) {
            obj.Set("text", Napi::String::New(env, event.text));
        } else {
            obj.Set("text", env.Undefined());
        }
    }

    return obj;
}

// Helper to parse Event from JS object
Event EventFromObject(const Napi::Object& obj) {
    Event event{};

    // Parse type
    std::string type_str = obj.Get("type").As<Napi::String>().Utf8Value();
    if (type_str == "mouseMove") event.type = EventType::MouseMove;
    else if (type_str == "mousePress") event.type = EventType::MousePress;
    else if (type_str == "mouseRelease") event.type = EventType::MouseRelease;
    else if (type_str == "keyPress") event.type = EventType::KeyPress;
    else if (type_str == "keyRelease") event.type = EventType::KeyRelease;

    // Parse state
    if (obj.Has("keyboardModifiers")) {
        event.state.keyboard_modifiers = obj.Get("keyboardModifiers").As<Napi::Number>().Uint32Value();
    }
    if (obj.Has("mouseButtons")) {
        event.state.mouse_buttons = obj.Get("mouseButtons").As<Napi::Number>().Uint32Value();
    }
    if (obj.Has("x")) {
        event.state.x = obj.Get("x").As<Napi::Number>().Int32Value();
    }
    if (obj.Has("y")) {
        event.state.y = obj.Get("y").As<Napi::Number>().Int32Value();
    }

    // Parse button for mouse events
    if (obj.Has("button")) {
        uint32_t button_val = obj.Get("button").As<Napi::Number>().Uint32Value();
        event.button = static_cast<MouseButton>(button_val);
    }

    // Parse keycode and text for key events
    if (obj.Has("keycode")) {
        event.keycode = obj.Get("keycode").As<Napi::Number>().Uint32Value();
    }
    if (obj.Has("text") && !obj.Get("text").IsUndefined()) {
        event.text = obj.Get("text").As<Napi::String>().Utf8Value();
    }

    event.timestamp = GetTimestamp();

    return event;
}

// KonfliktNative implementation
Napi::Object KonfliktNative::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "KonfliktNative", {
        InstanceAccessor<&KonfliktNative::GetDesktop>("desktop"),
        InstanceAccessor<&KonfliktNative::GetState>("state"),
        InstanceMethod<&KonfliktNative::On>("on"),
        InstanceMethod<&KonfliktNative::Off>("off"),
        InstanceMethod<&KonfliktNative::SendMouseEvent>("sendMouseEvent"),
        InstanceMethod<&KonfliktNative::SendKeyEvent>("sendKeyEvent")
    });

    Napi::FunctionReference* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("KonfliktNative", func);
    return exports;
}

KonfliktNative::KonfliktNative(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<KonfliktNative>(info) {

    platform_hook_ = CreatePlatformHook();

    if (!platform_hook_) {
        Napi::Error::New(info.Env(), "Failed to create platform hook").ThrowAsJavaScriptException();
        return;
    }

    if (!platform_hook_->initialize()) {
        Napi::Error::New(info.Env(), "Failed to initialize platform hook").ThrowAsJavaScriptException();
        return;
    }

    // Set up callback for platform events
    platform_hook_->event_callback = [this](const Event& event) {
        HandlePlatformEvent(event);
    };

    // Don't start listening immediately - wait for user to register listeners
}

KonfliktNative::~KonfliktNative() {
    if (platform_hook_) {
        platform_hook_->stop_listening();
        platform_hook_->shutdown();
    }
}

Napi::Value KonfliktNative::GetDesktop(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Desktop desktop = platform_hook_->get_desktop();
    return DesktopToObject(env, desktop);
}

Napi::Value KonfliktNative::GetState(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    State state = platform_hook_->get_state();
    return StateToObject(env, state);
}

void KonfliktNative::On(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected (type: string, listener: function)").ThrowAsJavaScriptException();
        return;
    }

    std::string type_str = info[0].As<Napi::String>().Utf8Value();
    Napi::Function listener = info[1].As<Napi::Function>();

    // Convert type string to EventType
    EventType type;
    if (type_str == "mouseMove") type = EventType::MouseMove;
    else if (type_str == "mousePress") type = EventType::MousePress;
    else if (type_str == "mouseRelease") type = EventType::MouseRelease;
    else if (type_str == "keyPress") type = EventType::KeyPress;
    else if (type_str == "keyRelease") type = EventType::KeyRelease;
    else if (type_str == "desktopChanged") type = EventType::DesktopChanged;
    else {
        Napi::TypeError::New(env, "Unknown event type").ThrowAsJavaScriptException();
        return;
    }

    // Create thread-safe function
    auto tsfn = Napi::ThreadSafeFunction::New(
        env,
        listener,
        "KonfliktEventListener",
        0,
        1
    );

    listeners_[type].listeners.push_back(std::move(tsfn));

    // Start listening if not already started
    if (!is_listening_ && platform_hook_) {
        platform_hook_->start_listening();
        is_listening_ = true;
    }
}

void KonfliktNative::Off(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected (type: string, listener: function)").ThrowAsJavaScriptException();
        return;
    }

    std::string type_str = info[0].As<Napi::String>().Utf8Value();

    // Convert type string to EventType
    EventType type;
    if (type_str == "mouseMove") type = EventType::MouseMove;
    else if (type_str == "mousePress") type = EventType::MousePress;
    else if (type_str == "mouseRelease") type = EventType::MouseRelease;
    else if (type_str == "keyPress") type = EventType::KeyPress;
    else if (type_str == "keyRelease") type = EventType::KeyRelease;
    else if (type_str == "desktopChanged") type = EventType::DesktopChanged;
    else {
        return;
    }

    // Remove listener (simplified - in real implementation would need to track by function reference)
    auto it = listeners_.find(type);
    if (it != listeners_.end() && !it->second.listeners.empty()) {
        it->second.listeners.back().Release();
        it->second.listeners.pop_back();
    }
}

void KonfliktNative::SendMouseEvent(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected event object").ThrowAsJavaScriptException();
        return;
    }

    Event event = EventFromObject(info[0].As<Napi::Object>());
    platform_hook_->send_mouse_event(event);
}

void KonfliktNative::SendKeyEvent(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected event object").ThrowAsJavaScriptException();
        return;
    }

    Event event = EventFromObject(info[0].As<Napi::Object>());
    platform_hook_->send_key_event(event);
}

void KonfliktNative::HandlePlatformEvent(const Event& event) {
    auto it = listeners_.find(event.type);
    if (it == listeners_.end()) {
        return;
    }

    for (auto& tsfn : it->second.listeners) {
        // Call the thread-safe function with the event
        tsfn.BlockingCall([event](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({EventToObject(env, event)});
        });
    }
}

} // namespace konflikt

// Module initialization
Napi::Object InitModule(Napi::Env env, Napi::Object exports) {
    return konflikt::KonfliktNative::Init(env, exports);
}

NODE_API_MODULE(konflikt_native, InitModule)
