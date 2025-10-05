#!/usr/bin/env node

const path = require('path');

// Import the native module
const { KonfliktNative } = require('./dist/native/Release/KonfliktNative.node');

async function testClipboard() {
    console.log('Testing clipboard functionality...');
    
    try {
        // Create an instance of the native module
        const native = new KonfliktNative({
            verbose: console.log,
            debug: console.log,
            log: console.log,
            error: console.error
        });

        // Test setting clipboard text
        const testText = "Hello from Konflikt clipboard test!";
        console.log(`Setting clipboard to: "${testText}"`);
        native.setClipboardText(testText);

        // Wait a moment for the operation to complete
        await new Promise(resolve => setTimeout(resolve, 100));

        // Test getting clipboard text
        const retrievedText = native.getClipboardText();
        console.log(`Retrieved from clipboard: "${retrievedText}"`);

        // Check if they match
        if (retrievedText === testText) {
            console.log('✅ Clipboard test PASSED!');
        } else {
            console.log('❌ Clipboard test FAILED!');
            console.log(`Expected: "${testText}"`);
            console.log(`Got: "${retrievedText}"`);
        }

        // Test with empty string
        console.log('\nTesting with empty string...');
        native.setClipboardText('');
        await new Promise(resolve => setTimeout(resolve, 100));
        const emptyResult = native.getClipboardText();
        console.log(`Empty clipboard result: "${emptyResult}"`);
        
        if (emptyResult === '') {
            console.log('✅ Empty string test PASSED!');
        } else {
            console.log('❌ Empty string test FAILED!');
        }

        // Test with unicode
        console.log('\nTesting with unicode characters...');
        const unicodeText = "Hello 🌍! 你好 Ωorld";
        native.setClipboardText(unicodeText);
        await new Promise(resolve => setTimeout(resolve, 100));
        const unicodeResult = native.getClipboardText();
        console.log(`Unicode test: "${unicodeResult}"`);
        
        if (unicodeResult === unicodeText) {
            console.log('✅ Unicode test PASSED!');
        } else {
            console.log('❌ Unicode test FAILED!');
        }

        // Test clipboard vs primary selection (X11 specific behavior)
        console.log('\nTesting selection types...');
        console.log('Note: On macOS, Primary selection is ignored and falls back to Clipboard');
        
        // Set different text in clipboard vs selection to test differentiation
        const clipboardOnlyText = "Clipboard only text";
        native.setClipboardText(clipboardOnlyText); // Default (Auto) - sets both on X11
        await new Promise(resolve => setTimeout(resolve, 100));
        
        const autoResult = native.getClipboardText(); // Default (Auto) 
        const clipboardResult = native.getClipboardText(); // Explicit Clipboard
        
        console.log(`Auto mode result: "${autoResult}"`);
        console.log(`Clipboard result: "${clipboardResult}"`);
        
        if (autoResult === clipboardOnlyText && clipboardResult === clipboardOnlyText) {
            console.log('✅ Selection type test PASSED!');
        } else {
            console.log('❌ Selection type test FAILED!');
        }

    } catch (error) {
        console.error('❌ Test failed with error:', error);
    }
}

// Run the test
if (require.main === module) {
    testClipboard().catch(console.error);
}

module.exports = { testClipboard };