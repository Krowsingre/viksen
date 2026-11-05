import Foundation
import OSLog

/// Runtime log for diagnosing problems on real devices. Writes to the unified
/// system log (visible in Console.app) and to a small rotating file in the app's
/// Library/Logs folder, which the user can view and share from Settings.
/// Privacy: events only. Never audio, health values, location or file contents.
enum AppLog {
    nonisolated(unsafe) static var launchStarted: Date?
    private static let logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "app", category: "runtime")
    private static let queue = DispatchQueue(label: "app.log", qos: .utility)
    private static let maxBytes = 512 * 1024

    static let fileURL: URL = {
        let dir = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Logs", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("runtime.log")
    }()
    private static var previousURL: URL { fileURL.deletingPathExtension().appendingPathExtension("1.log") }

    private static let stamp: ISO8601DateFormatter = {
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return f
    }()

    static func info(_ message: String) { write("INFO", message); logger.info("\(message, privacy: .public)") }
    static func warning(_ message: String) { write("WARN", message); logger.warning("\(message, privacy: .public)") }
    static func error(_ message: String) { write("ERROR", message); logger.error("\(message, privacy: .public)") }

    private static func write(_ level: String, _ message: String) {
        let line = "\(stamp.string(from: Date())) \(level) \(message)\n"
        queue.async {
            let fm = FileManager.default
            if let size = (try? fm.attributesOfItem(atPath: fileURL.path)[.size] as? Int), size > maxBytes {
                try? fm.removeItem(at: previousURL)
                try? fm.moveItem(at: fileURL, to: previousURL)
            }
            guard let data = line.data(using: .utf8) else { return }
            if let h = try? FileHandle(forWritingTo: fileURL) {
                defer { try? h.close() }
                _ = try? h.seekToEnd()
                try? h.write(contentsOf: data)
            } else {
                try? data.write(to: fileURL)
            }
        }
    }

    /// Last `maxLines` lines (previous file included when the current one is short).
    static func tail(_ maxLines: Int = 400) -> [String] {
        queue.sync {
            var lines: [String] = []
            for url in [previousURL, fileURL] {
                if let text = try? String(contentsOf: url, encoding: .utf8) {
                    lines += text.split(separator: "\n", omittingEmptySubsequences: true).map(String.init)
                }
            }
            return Array(lines.suffix(maxLines))
        }
    }

    static func clear() {
        queue.sync {
            try? FileManager.default.removeItem(at: fileURL)
            try? FileManager.default.removeItem(at: previousURL)
        }
    }

    /// One-line device/app summary written at launch.
    static func launchBanner() -> String {
        let info = Bundle.main.infoDictionary ?? [:]
        let version = info["CFBundleShortVersionString"] as? String ?? "?"
        let build = info["CFBundleVersion"] as? String ?? "?"
        var sys = utsname()
        uname(&sys)
        let model = withUnsafePointer(to: &sys.machine) {
            $0.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
        }
        let os = ProcessInfo.processInfo.operatingSystemVersionString
        return "Launch v\(version) (\(build)) on \(model), \(Platform.deviceNoun) \(os)"
    }
}

/// How this copy of the app was installed.
enum AppBuild {
    /// A TestFlight (or development) install: its receipt is a sandbox receipt.
    static let isTestFlight: Bool = {
        #if DEBUG
        return true
        #else
        return Bundle.main.appStoreReceiptURL?.lastPathComponent == "sandboxReceipt"
        #endif
    }()
}

/// Records main-thread stalls in the diagnostics log (owner reported freezes): a
/// background thread pings the main thread every 0.5 s and logs any reply slower than
/// 250 ms, with how long it took. Cheap, and shares nothing but timing.
enum HangWatch {
    private static var started = false

    static func start() {
        guard !started else { return }
        started = true
        let thread = Thread {
            while true {
                Thread.sleep(forTimeInterval: 0.5)
                let sent = Date()
                let done = DispatchSemaphore(value: 0)
                DispatchQueue.main.async { done.signal() }
                if done.wait(timeout: .now() + 0.25) == .timedOut {
                    done.wait()
                    let ms = Int(Date().timeIntervalSince(sent) * 1000)
                    AppLog.warning("Main thread stalled \(ms) ms")
                }
            }
        }
        thread.name = "HangWatch"
        thread.qualityOfService = .utility
        thread.start()
    }
}

/// Times a block of work into the log when it takes noticeable time.
@discardableResult
func logTiming<T>(_ label: String, threshold: Double = 0.05, _ work: () throws -> T) rethrows -> T {
    let start = Date()
    defer {
        let s = Date().timeIntervalSince(start)
        if s >= threshold { AppLog.info(String(format: "Timing: %@ %.0f ms", label, s * 1000)) }
    }
    return try work()
}
