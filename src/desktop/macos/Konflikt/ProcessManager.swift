import Foundation

protocol ProcessManagerDelegate: AnyObject {
    func processDidStart()
    func processDidConnect(to server: String)
    func processDidDisconnect()
    func processDidFail(with error: String)
    func processDidTerminate(exitCode: Int32)
    func processPortChanged(to port: Int)
}

class ProcessManager {
    private var process: Process?
    private var outputPipe: Pipe?
    private var errorPipe: Pipe?
    private var outputBuffer: String = ""

    weak var delegate: ProcessManagerDelegate?

    var connectionStatus: (status: ConnectionStatus, serverName: String?) = (.disconnected, nil)
    private var currentPort: Int = 3000

    private let nodeExecutablePaths = [
        "/usr/local/bin/node",
        "/opt/homebrew/bin/node",
        "/usr/bin/node",
        "~/.nvm/versions/node/*/bin/node"  // NVM installations
    ]

    func startProcess(backendPath: String, role: String, serverAddress: String? = nil, port: Int? = nil, verbose: Bool = false) {
        guard process == nil else {
            print("Process already running")
            return
        }

        guard let nodePath = findNodeExecutable() else {
            delegate?.processDidFail(with: "Node.js not found")
            return
        }

        process = Process()
        process?.executableURL = URL(fileURLWithPath: nodePath)

        var arguments = [backendPath, "--role=\(role)"]

        if let server = serverAddress, !server.isEmpty, role == "client" {
            arguments.append("--server=\(server)")
        }

        if let port = port {
            arguments.append("--port=\(port)")
            currentPort = port
        }

        if verbose {
            arguments.append("-vv")
        }

        process?.arguments = arguments

        // Set up environment
        var environment = ProcessInfo.processInfo.environment
        // Ensure we have a proper PATH
        if environment["PATH"] == nil {
            environment["PATH"] = "/usr/local/bin:/usr/bin:/bin:/opt/homebrew/bin"
        }
        process?.environment = environment

        // Set working directory to the backend directory
        let backendDir = (backendPath as NSString).deletingLastPathComponent
        let projectDir = ((backendDir as NSString).deletingLastPathComponent as NSString).deletingLastPathComponent
        process?.currentDirectoryURL = URL(fileURLWithPath: projectDir)

        // Set up pipes for output
        outputPipe = Pipe()
        errorPipe = Pipe()
        process?.standardOutput = outputPipe
        process?.standardError = errorPipe

        // Handle output
        outputPipe?.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if let output = String(data: data, encoding: .utf8), !output.isEmpty {
                self?.handleOutput(output)
            }
        }

        errorPipe?.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if let output = String(data: data, encoding: .utf8), !output.isEmpty {
                self?.handleOutput(output)
            }
        }

        // Handle termination
        process?.terminationHandler = { [weak self] process in
            DispatchQueue.main.async {
                self?.handleTermination(exitCode: process.terminationStatus)
            }
        }

        do {
            try process?.run()
            delegate?.processDidStart()
            connectionStatus = (.connecting, nil)
        } catch {
            delegate?.processDidFail(with: "Failed to start: \(error.localizedDescription)")
            process = nil
        }
    }

    func stopProcess() {
        outputPipe?.fileHandleForReading.readabilityHandler = nil
        errorPipe?.fileHandleForReading.readabilityHandler = nil

        if let process = process, process.isRunning {
            process.terminate()
            // Give it a moment to terminate gracefully
            DispatchQueue.global().asyncAfter(deadline: .now() + 2.0) {
                if process.isRunning {
                    process.interrupt()  // Force kill
                }
            }
        }
        process = nil
        connectionStatus = (.disconnected, nil)
    }

    private func findNodeExecutable() -> String? {
        for pathPattern in nodeExecutablePaths {
            let expandedPath = NSString(string: pathPattern).expandingTildeInPath

            // Handle glob patterns
            if expandedPath.contains("*") {
                let glob = Glob(pattern: expandedPath)
                if let firstMatch = glob.paths.first, FileManager.default.isExecutableFile(atPath: firstMatch) {
                    return firstMatch
                }
            } else if FileManager.default.isExecutableFile(atPath: expandedPath) {
                return expandedPath
            }
        }

        // Try using `which` as fallback
        let whichProcess = Process()
        whichProcess.executableURL = URL(fileURLWithPath: "/usr/bin/which")
        whichProcess.arguments = ["node"]
        let pipe = Pipe()
        whichProcess.standardOutput = pipe

        do {
            try whichProcess.run()
            whichProcess.waitUntilExit()
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            if let path = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines),
               !path.isEmpty {
                return path
            }
        } catch {
            print("Failed to find node via which: \(error)")
        }

        return nil
    }

    private func handleOutput(_ output: String) {
        // Append to buffer and process line by line
        outputBuffer += output

        while let newlineIndex = outputBuffer.firstIndex(of: "\n") {
            let line = String(outputBuffer[..<newlineIndex])
            outputBuffer = String(outputBuffer[outputBuffer.index(after: newlineIndex)...])
            processLogLine(line)
        }
    }

    private func processLogLine(_ line: String) {
        // Parse log output to detect status changes
        // Example log formats:
        // [INFO] Connected to server-name
        // [INFO] Handshake completed with peer: server-name
        // [ERROR] Connection failed

        if line.contains("Handshake completed with peer:") {
            if let serverMatch = line.range(of: "peer: ([^\\s]+)", options: .regularExpression) {
                let serverName = String(line[serverMatch]).replacingOccurrences(of: "peer: ", with: "")
                DispatchQueue.main.async {
                    self.connectionStatus = (.connected, serverName)
                    self.delegate?.processDidConnect(to: serverName)
                }
            }
        } else if line.contains("Disconnected from peer") || line.contains("Connection closed") {
            DispatchQueue.main.async {
                self.connectionStatus = (.disconnected, nil)
                self.delegate?.processDidDisconnect()
            }
        } else if line.contains("HTTP listening at") {
            // Extract port if mentioned
            if let portMatch = line.range(of: ":(\\d+)", options: .regularExpression) {
                let portStr = String(line[portMatch]).replacingOccurrences(of: ":", with: "")
                if let port = Int(portStr) {
                    DispatchQueue.main.async {
                        self.currentPort = port
                        self.delegate?.processPortChanged(to: port)
                    }
                }
            }
        } else if line.contains("[ERROR]") {
            // Could show errors in UI
            print("Backend error: \(line)")
        }

        // Print all output for debugging
        print("[Backend] \(line)")
    }

    private func handleTermination(exitCode: Int32) {
        outputPipe?.fileHandleForReading.readabilityHandler = nil
        errorPipe?.fileHandleForReading.readabilityHandler = nil
        process = nil
        connectionStatus = (.disconnected, nil)
        delegate?.processDidTerminate(exitCode: exitCode)
    }
}

// Simple glob implementation for finding node in NVM directories
class Glob {
    var paths: [String] = []

    init(pattern: String) {
        let baseDir = (pattern as NSString).deletingLastPathComponent
        let filePattern = (pattern as NSString).lastPathComponent

        guard let enumerator = FileManager.default.enumerator(atPath: baseDir) else {
            return
        }

        while let element = enumerator.nextObject() as? String {
            let fullPath = (baseDir as NSString).appendingPathComponent(element)
            if matchesPattern(element, pattern: filePattern) {
                paths.append(fullPath)
            }
        }

        paths.sort()
    }

    private func matchesPattern(_ string: String, pattern: String) -> Bool {
        // Simple wildcard matching
        let regexPattern = "^" + NSRegularExpression.escapedPattern(for: pattern)
            .replacingOccurrences(of: "\\*", with: ".*") + "$"
        return string.range(of: regexPattern, options: .regularExpression) != nil
    }
}
