#!/usr/bin/env node

const path = require("path");
const fs = require("fs");

// Import the native module
const { KonfliktNative } = require("./dist/native/Release/KonfliktNative.node");

async function testBinaryClipboard() {
    console.log("Testing binary clipboard functionality...");

    try {
        // Create an instance of the native module
        const native = new KonfliktNative({
            verbose: console.log,
            debug: console.log,
            log: console.log,
            error: console.error
        });

        // Test 1: Text data using binary API
        console.log("\n=== Test 1: Text data via binary API ===");
        const textData = "Hello binary clipboard world! 🎯";
        const textBuffer = Buffer.from(textData, "utf8");

        console.log(`Setting text via binary API: "${textData}"`);
        native.setClipboardData("text/plain", textBuffer);

        await new Promise((resolve) => setTimeout(resolve, 100));

        const retrievedTextBuffer = native.getClipboardData("text/plain");
        const retrievedText = retrievedTextBuffer.toString("utf8");

        console.log(`Retrieved: "${retrievedText}"`);
        if (retrievedText === textData) {
            console.log("✅ Binary text API test PASSED!");
        } else {
            console.log("❌ Binary text API test FAILED!");
        }

        // Test 2: Check available MIME types
        console.log("\n=== Test 2: Available MIME types ===");
        const mimeTypes = native.getClipboardMimeTypes();
        console.log("Available MIME types:", mimeTypes);

        if (mimeTypes.includes("text/plain")) {
            console.log("✅ MIME types test PASSED!");
        } else {
            console.log("❌ MIME types test FAILED!");
        }

        // Test 3: Binary image data (create a simple bitmap)
        console.log("\n=== Test 3: Binary image data ===");

        // Create a simple PNG-like binary data (not a real PNG, just test data)
        const imageData = Buffer.from([
            0x89,
            0x50,
            0x4e,
            0x47,
            0x0d,
            0x0a,
            0x1a,
            0x0a, // PNG signature
            0x00,
            0x00,
            0x00,
            0x0d,
            0x49,
            0x48,
            0x44,
            0x52, // IHDR chunk start
            0x00,
            0x00,
            0x00,
            0x01,
            0x00,
            0x00,
            0x00,
            0x01, // 1x1 pixel
            0x08,
            0x02,
            0x00,
            0x00,
            0x00,
            0x90,
            0x77,
            0x53, // bit depth, color type, etc.
            0xde // dummy data
        ]);

        console.log(`Setting ${imageData.length} bytes of binary image data`);
        native.setClipboardData("image/png", imageData);

        await new Promise((resolve) => setTimeout(resolve, 100));

        const retrievedImageData = native.getClipboardData("image/png");
        console.log(`Retrieved ${retrievedImageData.length} bytes of image data`);

        if (Buffer.compare(imageData, retrievedImageData) === 0) {
            console.log("✅ Binary image data test PASSED!");
        } else {
            console.log("❌ Binary image data test FAILED!");
            console.log("Expected:", imageData);
            console.log("Got:", retrievedImageData);
        }

        // Test 4: Multiple MIME types
        console.log("\n=== Test 4: Multiple MIME types ===");
        const jsonData = JSON.stringify({ hello: "world", emoji: "🌍" });
        const jsonBuffer = Buffer.from(jsonData, "utf8");

        console.log(`Setting JSON data: ${jsonData}`);
        native.setClipboardData("application/json", jsonBuffer);

        await new Promise((resolve) => setTimeout(resolve, 100));

        const allMimeTypes = native.getClipboardMimeTypes();
        console.log("All available MIME types:", allMimeTypes);

        const retrievedJsonBuffer = native.getClipboardData("application/json");
        const retrievedJson = retrievedJsonBuffer.toString("utf8");

        console.log(`Retrieved JSON: ${retrievedJson}`);

        if (retrievedJson === jsonData) {
            console.log("✅ Multiple MIME types test PASSED!");
        } else {
            console.log("❌ Multiple MIME types test FAILED!");
        }

        // Test 5: Selection types with binary data
        console.log("\n=== Test 5: Selection types with binary data ===");
        const clipboardOnlyData = Buffer.from("Clipboard only binary data", "utf8");

        console.log("Setting data to clipboard selection only...");
        native.setClipboardData("text/plain", clipboardOnlyData, "clipboard");

        await new Promise((resolve) => setTimeout(resolve, 100));

        const clipboardResult = native.getClipboardData("text/plain", "clipboard");
        const autoResult = native.getClipboardData("text/plain"); // Auto mode

        console.log(`Clipboard result: "${clipboardResult.toString("utf8")}"`);
        console.log(`Auto result: "${autoResult.toString("utf8")}"`);

        if (
            Buffer.compare(clipboardOnlyData, clipboardResult) === 0 &&
            Buffer.compare(clipboardOnlyData, autoResult) === 0
        ) {
            console.log("✅ Selection types with binary data test PASSED!");
        } else {
            console.log("❌ Selection types with binary data test FAILED!");
        }

        console.log("\n🎉 Binary clipboard testing completed!");
    } catch (error) {
        console.error("❌ Test failed with error:", error);
    }
}

// Run the test
if (require.main === module) {
    testBinaryClipboard().catch(console.error);
}

module.exports = { testBinaryClipboard };
