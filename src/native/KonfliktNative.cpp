#include "KonfliktNative.h"

#include <chrono>

namespace konflikt {

// MIME type mapping implementation
std::string MimeTypeMapper::mimeToMacType(const std::string &mimeType) {
    static const std::unordered_map<std::string, std::string> mimeToMac = {
        // Text types
        {"text/plain", "public.utf8-plain-text"},
        {"text/plain;charset=utf-8", "public.utf8-plain-text"},
        {"text/html", "public.html"},
        {"text/rtf", "public.rtf"},
        {"text/uri-list", "public.file-url"},
        
        // Image types
        {"image/png", "public.png"},
        {"image/jpeg", "public.jpeg"},
        {"image/jpg", "public.jpeg"}, 
        {"image/gif", "com.compuserve.gif"},
        {"image/tiff", "public.tiff"},
        {"image/bmp", "com.microsoft.bmp"},
        {"image/webp", "org.webmproject.webp"},
        {"image/svg+xml", "public.svg-image"},
        
        // Document types
        {"application/pdf", "com.adobe.pdf"},
        {"application/postscript", "com.adobe.postscript"},
        {"application/rtf", "public.rtf"},
        
        // Archive types
        {"application/zip", "public.zip-archive"},
        {"application/x-tar", "public.tar-archive"},
        {"application/gzip", "org.gnu.gnu-zip-archive"},
        
        // Audio types
        {"audio/mpeg", "public.mp3"},
        {"audio/wav", "com.microsoft.waveform-audio"},
        {"audio/aac", "public.aac-audio"},
        {"audio/flac", "org.xiph.flac"},
        
        // Video types  
        {"video/mp4", "public.mpeg-4"},
        {"video/quicktime", "com.apple.quicktime-movie"},
        {"video/avi", "public.avi"},
        
        // Data types
        {"application/json", "public.json"},
        {"application/xml", "public.xml"},
        {"text/csv", "public.comma-separated-values-text"},
        {"text/tab-separated-values", "public.tab-separated-values-text"},
    };
    
    auto it = mimeToMac.find(mimeType);
    return (it != mimeToMac.end()) ? it->second : mimeType;
}

std::string MimeTypeMapper::mimeToX11Type(const std::string &mimeType) {
    // X11 typically uses MIME types directly, but has some special cases
    static const std::unordered_map<std::string, std::string> mimeToX11 = {
        {"text/plain", "UTF8_STRING"},
        {"text/plain;charset=utf-8", "UTF8_STRING"},
        {"text/uri-list", "text/uri-list"},
    };
    
    auto it = mimeToX11.find(mimeType);
    return (it != mimeToX11.end()) ? it->second : mimeType;
}

std::string MimeTypeMapper::macTypeToMime(const std::string &macType) {
    static const std::unordered_map<std::string, std::string> macToMime = {
        // Text types
        {"public.utf8-plain-text", "text/plain"},
        {"public.plain-text", "text/plain"},
        {"public.html", "text/html"},
        {"public.rtf", "text/rtf"},
        {"public.file-url", "text/uri-list"},
        
        // Image types
        {"public.png", "image/png"},
        {"public.jpeg", "image/jpeg"},
        {"com.compuserve.gif", "image/gif"},
        {"public.tiff", "image/tiff"},
        {"com.microsoft.bmp", "image/bmp"},
        {"org.webmproject.webp", "image/webp"},
        {"public.svg-image", "image/svg+xml"},
        
        // Document types
        {"com.adobe.pdf", "application/pdf"},
        {"com.adobe.postscript", "application/postscript"},
        
        // Archive types
        {"public.zip-archive", "application/zip"},
        {"public.tar-archive", "application/x-tar"},
        {"org.gnu.gnu-zip-archive", "application/gzip"},
        
        // Audio types
        {"public.mp3", "audio/mpeg"},
        {"com.microsoft.waveform-audio", "audio/wav"},
        {"public.aac-audio", "audio/aac"},
        {"org.xiph.flac", "audio/flac"},
        
        // Video types
        {"public.mpeg-4", "video/mp4"},
        {"com.apple.quicktime-movie", "video/quicktime"},
        {"public.avi", "video/avi"},
        
        // Data types
        {"public.json", "application/json"},
        {"public.xml", "application/xml"},
        {"public.comma-separated-values-text", "text/csv"},
        {"public.tab-separated-values-text", "text/tab-separated-values"},
    };
    
    auto it = macToMime.find(macType);
    return (it != macToMime.end()) ? it->second : macType;
}

std::string MimeTypeMapper::x11TypeToMime(const std::string &x11Type) {
    static const std::unordered_map<std::string, std::string> x11ToMime = {
        {"UTF8_STRING", "text/plain"},
        {"STRING", "text/plain"},
        {"TEXT", "text/plain"},
        {"text/uri-list", "text/uri-list"},
    };
    
    auto it = x11ToMime.find(x11Type);
    return (it != x11ToMime.end()) ? it->second : x11Type;
}

std::vector<std::string> MimeTypeMapper::getSupportedMimeTypes() {
    return {
        // Text types
        "text/plain", "text/html", "text/rtf", "text/uri-list", "text/csv", "text/tab-separated-values",
        
        // Image types  
        "image/png", "image/jpeg", "image/jpg", "image/gif", "image/tiff", "image/bmp", 
        "image/webp", "image/svg+xml",
        
        // Document types
        "application/pdf", "application/postscript", "application/rtf",
        
        // Archive types
        "application/zip", "application/x-tar", "application/gzip",
        
        // Audio types
        "audio/mpeg", "audio/wav", "audio/aac", "audio/flac",
        
        // Video types
        "video/mp4", "video/quicktime", "video/avi",
        
        // Data types
        "application/json", "application/xml"
    };
}

// Helper to get current timestamp in milliseconds
uint64_t timestamp()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch())
        .count();
}

// Helper to create State object
Napi::Object stateToObject(Napi::Env env, const State &state)
{
    auto obj = Napi::Object::New(env);
    obj.Set("keyboardModifiers", Napi::Number::New(env, state.keyboardModifiers));
    obj.Set("mouseButtons", Napi::Number::New(env, state.mouseButtons));
    obj.Set("x", Napi::Number::New(env, state.x));
    obj.Set("y", Napi::Number::New(env, state.y));
    return obj;
}

// Helper to create Desktop object
Napi::Object desktopToObject(Napi::Env env, const Desktop &desktop)
{
    auto obj = Napi::Object::New(env);
    obj.Set("width", Napi::Number::New(env, desktop.width));
    obj.Set("height", Napi::Number::New(env, desktop.height));
    return obj;
}

// Helper to create Event object
Napi::Object eventToObject(Napi::Env env, const Event &event)
{
    auto obj = Napi::Object::New(env);

    // Set type
    std::string typeStr;
    switch (event.type) {
        case EventType::MouseMove: typeStr = "mouseMove"; break;
        case EventType::MousePress: typeStr = "mousePress"; break;
        case EventType::MouseRelease: typeStr = "mouseRelease"; break;
        case EventType::KeyPress: typeStr = "keyPress"; break;
        case EventType::KeyRelease: typeStr = "keyRelease"; break;
        case EventType::DesktopChanged: typeStr = "desktopChanged"; break;
    }
    obj.Set("type", Napi::String::New(env, typeStr));

    // Set timestamp
    obj.Set("timestamp", Napi::Number::New(env, static_cast<double>(event.timestamp)));

    // Set state
    obj.Set("keyboardModifiers", Napi::Number::New(env, event.state.keyboardModifiers));
    obj.Set("mouseButtons", Napi::Number::New(env, event.state.mouseButtons));
    obj.Set("x", Napi::Number::New(env, event.state.x));
    obj.Set("y", Napi::Number::New(env, event.state.y));

    // Set button for mouse button events
    if (event.type == EventType::MousePress || event.type == EventType::MouseRelease) {
        obj.Set("button", Napi::Number::New(env, toUInt32(event.button)));
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
Event eventFromObject(const Napi::Object &obj)
{
    Event event {};

    // Parse type
    std::string typeStr = obj.Get("type").As<Napi::String>().Utf8Value();
    if (typeStr == "mouseMove") {
        event.type = EventType::MouseMove;
    } else if (typeStr == "mousePress") {
        event.type = EventType::MousePress;
    } else if (typeStr == "mouseRelease") {
        event.type = EventType::MouseRelease;
    } else if (typeStr == "keyPress") {
        event.type = EventType::KeyPress;
    } else if (typeStr == "keyRelease") {
        event.type = EventType::KeyRelease;
    }

    // Parse state
    if (obj.Has("keyboardModifiers")) {
        event.state.keyboardModifiers = obj.Get("keyboardModifiers").As<Napi::Number>().Uint32Value();
    }
    if (obj.Has("mouseButtons")) {
        event.state.mouseButtons = obj.Get("mouseButtons").As<Napi::Number>().Uint32Value();
    }
    if (obj.Has("x")) {
        event.state.x = obj.Get("x").As<Napi::Number>().Int32Value();
    }
    if (obj.Has("y")) {
        event.state.y = obj.Get("y").As<Napi::Number>().Int32Value();
    }

    // Parse button for mouse events
    if (obj.Has("button")) {
        uint32_t buttonVal = obj.Get("button").As<Napi::Number>().Uint32Value();
        event.button       = static_cast<MouseButton>(buttonVal);
    }

    // Parse keycode and text for key events
    if (obj.Has("keycode")) {
        event.keycode = obj.Get("keycode").As<Napi::Number>().Uint32Value();
    }
    if (obj.Has("text") && !obj.Get("text").IsUndefined()) {
        event.text = obj.Get("text").As<Napi::String>().Utf8Value();
    }

    event.timestamp = timestamp();

    return event;
}

// KonfliktNative implementation
Napi::Object KonfliktNative::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "KonfliktNative", { InstanceAccessor<&KonfliktNative::GetDesktop>("desktop"), InstanceAccessor<&KonfliktNative::GetState>("state"), InstanceMethod<&KonfliktNative::On>("on"), InstanceMethod<&KonfliktNative::Off>("off"), InstanceMethod<&KonfliktNative::SendMouseEvent>("sendMouseEvent"), InstanceMethod<&KonfliktNative::SendKeyEvent>("sendKeyEvent"), InstanceMethod<&KonfliktNative::showCursor>("showCursor"), InstanceMethod<&KonfliktNative::hideCursor>("hideCursor"), InstanceAccessor<&KonfliktNative::isCursorVisible>("isCursorVisible"), InstanceMethod<&KonfliktNative::getClipboardText>("getClipboardText"), InstanceMethod<&KonfliktNative::setClipboardText>("setClipboardText"), InstanceMethod<&KonfliktNative::getClipboardData>("getClipboardData"), InstanceMethod<&KonfliktNative::setClipboardData>("setClipboardData"), InstanceMethod<&KonfliktNative::getClipboardMimeTypes>("getClipboardMimeTypes") });

    Napi::FunctionReference *constructor = new Napi::FunctionReference();
    *constructor                         = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("KonfliktNative", func);
    return exports;
}

KonfliktNative::KonfliktNative(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<KonfliktNative>(info)
{
    // Parse logger callbacks if provided
    if (info.Length() > 0 && info[0].IsObject()) {
        Napi::Object loggerObj = info[0].As<Napi::Object>();

        // Create thread-safe functions for each log level
        if (loggerObj.Has("verbose") && loggerObj.Get("verbose").IsFunction()) {
            auto verboseFunc = loggerObj.Get("verbose").As<Napi::Function>();
            mVerboseTsfn     = Napi::ThreadSafeFunction::New(
                info.Env(),
                verboseFunc,
                "VerboseLogger",
                0,
                1,
                [this](Napi::Env) { /* Finalizer */ });
            mLogger.verbose = [this](const std::string &message) {
                if (mVerboseTsfn) {
                    mVerboseTsfn.BlockingCall(new std::string(message), [](Napi::Env env, Napi::Function jsCallback, std::string *data) {
                        jsCallback.Call({ Napi::String::New(env, *data) });
                        delete data;
                    });
                }
            };
        }
        if (loggerObj.Has("debug") && loggerObj.Get("debug").IsFunction()) {
            auto debugFunc = loggerObj.Get("debug").As<Napi::Function>();
            mDebugTsfn     = Napi::ThreadSafeFunction::New(
                info.Env(),
                debugFunc,
                "DebugLogger",
                0,
                1,
                [this](Napi::Env) { /* Finalizer */ });
            mLogger.debug = [this](const std::string &message) {
                if (mDebugTsfn) {
                    mDebugTsfn.BlockingCall(new std::string(message), [](Napi::Env env, Napi::Function jsCallback, std::string *data) {
                        jsCallback.Call({ Napi::String::New(env, *data) });
                        delete data;
                    });
                }
            };
        }
        if (loggerObj.Has("log") && loggerObj.Get("log").IsFunction()) {
            auto logFunc = loggerObj.Get("log").As<Napi::Function>();
            mLogTsfn     = Napi::ThreadSafeFunction::New(
                info.Env(),
                logFunc,
                "LogLogger",
                0,
                1,
                [this](Napi::Env) { /* Finalizer */ });
            mLogger.log = [this](const std::string &message) {
                if (mLogTsfn) {
                    mLogTsfn.BlockingCall(new std::string(message), [](Napi::Env env, Napi::Function jsCallback, std::string *data) {
                        jsCallback.Call({ Napi::String::New(env, *data) });
                        delete data;
                    });
                }
            };
        }
        if (loggerObj.Has("error") && loggerObj.Get("error").IsFunction()) {
            auto errorFunc = loggerObj.Get("error").As<Napi::Function>();
            mErrorTsfn     = Napi::ThreadSafeFunction::New(
                info.Env(),
                errorFunc,
                "ErrorLogger",
                0,
                1,
                [this](Napi::Env) { /* Finalizer */ });
            mLogger.error = [this](const std::string &message) {
                if (mErrorTsfn) {
                    mErrorTsfn.BlockingCall(new std::string(message), [](Napi::Env env, Napi::Function jsCallback, std::string *data) {
                        jsCallback.Call({ Napi::String::New(env, *data) });
                        delete data;
                    });
                }
            };
        }
    }

    mPlatformHook = createPlatformHook();

    if (!mPlatformHook) {
        if (mLogger.error) {
            mLogger.error("Failed to create platform hook");
        }
        Napi::Error::New(info.Env(), "Failed to create platform hook").ThrowAsJavaScriptException();
        return;
    }

    if (!mPlatformHook->initialize(mLogger)) {
        if (mLogger.error) {
            mLogger.error("Failed to initialize platform hook");
        }
        Napi::Error::New(info.Env(), "Failed to initialize platform hook").ThrowAsJavaScriptException();
        return;
    }

    // Set up callback for platform events
    mPlatformHook->eventCallback = [this](const Event &event) {
        handlePlatformEvent(event);
    };

    // Don't start listening immediately - wait for user to register listeners
}

KonfliktNative::~KonfliktNative()
{
    // Release all thread-safe functions first
    for (auto &pair : mListeners) {
        for (auto &entry : pair.second.listeners) {
            entry.tsfn.Release();
            entry.funcRef.Reset();
        }
        pair.second.listeners.clear();
    }
    mListeners.clear();

    // Release logger thread-safe functions
    if (mVerboseTsfn) {
        mVerboseTsfn.Release();
    }
    if (mDebugTsfn) {
        mDebugTsfn.Release();
    }
    if (mLogTsfn) {
        mLogTsfn.Release();
    }
    if (mErrorTsfn) {
        mErrorTsfn.Release();
    }

    // Then stop the platform hook
    if (mPlatformHook) {
        mPlatformHook->stopListening();
        mPlatformHook->shutdown();
    }
}

Napi::Value KonfliktNative::GetDesktop(const Napi::CallbackInfo &info)
{
    Napi::Env env   = info.Env();
    Desktop desktop = mPlatformHook->getDesktop();
    return desktopToObject(env, desktop);
}

Napi::Value KonfliktNative::GetState(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    State state   = mPlatformHook->getState();
    return stateToObject(env, state);
}

void KonfliktNative::On(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected (type: string, listener: function)").ThrowAsJavaScriptException();
        return;
    }

    std::string typeStr     = info[0].As<Napi::String>().Utf8Value();
    Napi::Function listener = info[1].As<Napi::Function>();

    // Convert type string to EventType
    EventType type;
    if (typeStr == "mouseMove") {
        type = EventType::MouseMove;
    } else if (typeStr == "mousePress") {
        type = EventType::MousePress;
    } else if (typeStr == "mouseRelease") {
        type = EventType::MouseRelease;
    } else if (typeStr == "keyPress") {
        type = EventType::KeyPress;
    } else if (typeStr == "keyRelease") {
        type = EventType::KeyRelease;
    } else if (typeStr == "desktopChanged") {
        type = EventType::DesktopChanged;
    }
    else {
        std::string validTypes = "Valid event types are: 'mouseMove', 'mousePress', 'mouseRelease', 'keyPress', 'keyRelease', 'desktopChanged'";
        Napi::TypeError::New(env, "Unknown event type '" + typeStr + "'. " + validTypes).ThrowAsJavaScriptException();
        return;
    }

    // Create thread-safe function
    auto tsfn = Napi::ThreadSafeFunction::New(
        env,
        listener,
        "KonfliktEventListener",
        0,
        1);

    // Create function reference for comparison
    Napi::FunctionReference funcRef = Napi::Persistent(listener);

    // Store both
    ListenerEntry entry;
    entry.tsfn = std::move(tsfn);
    entry.funcRef = std::move(funcRef);
    
    mListeners[type].listeners.push_back(std::move(entry));

    // Start listening if not already started
    if (!mIsListening && mPlatformHook) {
        mPlatformHook->startListening();
        mIsListening = true;
    }
}

void KonfliktNative::Off(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected (type: string, listener: function)").ThrowAsJavaScriptException();
        return;
    }

    std::string typeStr = info[0].As<Napi::String>().Utf8Value();
    Napi::Function targetListener = info[1].As<Napi::Function>();

    // Convert type string to EventType
    EventType type;
    if (typeStr == "mouseMove") {
        type = EventType::MouseMove;
    } else if (typeStr == "mousePress") {
        type = EventType::MousePress;
    } else if (typeStr == "mouseRelease") {
        type = EventType::MouseRelease;
    } else if (typeStr == "keyPress") {
        type = EventType::KeyPress;
    } else if (typeStr == "keyRelease") {
        type = EventType::KeyRelease;
    } else if (typeStr == "desktopChanged") {
        type = EventType::DesktopChanged;
    }
    else {
        return;
    }

    // Find and remove the specific listener
    auto it = mListeners.find(type);
    if (it != mListeners.end()) {
        auto &listeners = it->second.listeners;
        
        for (auto listenerIt = listeners.begin(); listenerIt != listeners.end(); ++listenerIt) {
            // Compare the function references
            if (listenerIt->funcRef.Value().StrictEquals(targetListener)) {
                // Release resources
                listenerIt->tsfn.Release();
                listenerIt->funcRef.Reset();
                
                // Remove from vector
                listeners.erase(listenerIt);
                break;
            }
        }
    }
}

void KonfliktNative::SendMouseEvent(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected event object").ThrowAsJavaScriptException();
        return;
    }

    Event event = eventFromObject(info[0].As<Napi::Object>());
    mPlatformHook->sendMouseEvent(event);
}

void KonfliktNative::SendKeyEvent(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected event object").ThrowAsJavaScriptException();
        return;
    }

    Event event = eventFromObject(info[0].As<Napi::Object>());
    mPlatformHook->sendKeyEvent(event);
}

void KonfliktNative::showCursor(const Napi::CallbackInfo &/*info*/)
{
    if (mPlatformHook) {
        mPlatformHook->showCursor();
    }
}

void KonfliktNative::hideCursor(const Napi::CallbackInfo &/*info*/)
{
    if (mPlatformHook) {
        mPlatformHook->hideCursor();
    }
}

Napi::Value KonfliktNative::isCursorVisible(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (mPlatformHook) {
        return Napi::Boolean::New(env, mPlatformHook->isCursorVisible());
    }

    return Napi::Boolean::New(env, true);
}

Napi::Value KonfliktNative::getClipboardText(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (mPlatformHook) {
        std::string text = mPlatformHook->getClipboardText();
        return Napi::String::New(env, text);
    }

    return Napi::String::New(env, "");
}

void KonfliktNative::setClipboardText(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected string argument").ThrowAsJavaScriptException();
        return;
    }

    std::string text = info[0].As<Napi::String>().Utf8Value();

    if (mPlatformHook) {
        mPlatformHook->setClipboardText(text);
    }
}

Napi::Value KonfliktNative::getClipboardData(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected mimeType string argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string mimeType = info[0].As<Napi::String>().Utf8Value();
    
    // Optional selection parameter
    ClipboardSelection selection = ClipboardSelection::Auto;
    if (info.Length() > 1 && info[1].IsString()) {
        std::string selectionStr = info[1].As<Napi::String>().Utf8Value();
        if (selectionStr == "clipboard") {
            selection = ClipboardSelection::Clipboard;
        } else if (selectionStr == "primary") {
            selection = ClipboardSelection::Primary;
        }
    }

    if (mPlatformHook) {
        std::vector<uint8_t> data = mPlatformHook->getClipboardData(mimeType, selection);
        
        // Return as Buffer
        return Napi::Buffer<uint8_t>::Copy(env, data.data(), data.size());
    }

    return env.Null();
}

void KonfliktNative::setClipboardData(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected (mimeType: string, data: Buffer, selection?: string)").ThrowAsJavaScriptException();
        return;
    }

    std::string mimeType = info[0].As<Napi::String>().Utf8Value();
    
    if (!info[1].IsBuffer()) {
        Napi::TypeError::New(env, "Expected Buffer for data argument").ThrowAsJavaScriptException();
        return;
    }

    Napi::Buffer<uint8_t> buffer = info[1].As<Napi::Buffer<uint8_t>>();
    std::vector<uint8_t> data(buffer.Data(), buffer.Data() + buffer.Length());

    // Optional selection parameter
    ClipboardSelection selection = ClipboardSelection::Auto;
    if (info.Length() > 2 && info[2].IsString()) {
        std::string selectionStr = info[2].As<Napi::String>().Utf8Value();
        if (selectionStr == "clipboard") {
            selection = ClipboardSelection::Clipboard;
        } else if (selectionStr == "primary") {
            selection = ClipboardSelection::Primary;
        }
    }

    if (mPlatformHook) {
        mPlatformHook->setClipboardData(mimeType, data, selection);
    }
}

Napi::Value KonfliktNative::getClipboardMimeTypes(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    // Optional selection parameter
    ClipboardSelection selection = ClipboardSelection::Auto;
    if (info.Length() > 0 && info[0].IsString()) {
        std::string selectionStr = info[0].As<Napi::String>().Utf8Value();
        if (selectionStr == "clipboard") {
            selection = ClipboardSelection::Clipboard;
        } else if (selectionStr == "primary") {
            selection = ClipboardSelection::Primary;
        }
    }

    if (mPlatformHook) {
        std::vector<std::string> mimeTypes = mPlatformHook->getClipboardMimeTypes(selection);
        
        Napi::Array result = Napi::Array::New(env, mimeTypes.size());
        for (size_t i = 0; i < mimeTypes.size(); ++i) {
            result[i] = Napi::String::New(env, mimeTypes[i]);
        }
        return result;
    }

    return Napi::Array::New(env, 0);
}

void KonfliktNative::handlePlatformEvent(const Event &event)
{
    if (!mIsListening) {
        return;
    }

    auto it = mListeners.find(event.type);
    if (it == mListeners.end() || it->second.listeners.empty()) {
        return;
    }

    for (auto &entry : it->second.listeners) {
        // Call the thread-safe function with the event
        auto status = entry.tsfn.NonBlockingCall([event](Napi::Env env, Napi::Function jsCallback) {
            try {
                jsCallback.Call({ eventToObject(env, event) });
            } catch (...) {
                // Silently ignore errors in callbacks
            }
        });

        // If the call fails repeatedly, it might mean the JS context is being destroyed
        // But we shouldn't stop listening on a single failure
        (void)status; // Ignore status for now
    }
}

} // namespace konflikt

// Module initialization
Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    return konflikt::KonfliktNative::Init(env, exports);
}

NODE_API_MODULE(KonfliktNative, InitModule)
