import SwiftUI

// Small shims so the same SwiftUI views build for iPhone and Mac. The look stays the
// same; only the platform's own conventions (title bars, toolbar slots) differ.

extension ToolbarItemPlacement {
    /// Leading edge of the bar on iPhone; the window's navigation slot on Mac.
    static var barLeading: ToolbarItemPlacement {
        #if os(iOS)
        .topBarLeading
        #else
        .navigation
        #endif
    }

    /// Trailing edge of the bar on iPhone; the window's primary toolbar slot on Mac.
    static var barTrailing: ToolbarItemPlacement {
        #if os(iOS)
        .topBarTrailing
        #else
        .primaryAction
        #endif
    }
}

extension View {
    /// Compact title on iPhone (no effect on Mac, which has no large titles).
    @ViewBuilder func inlineNavigationTitle() -> some View {
        #if os(iOS)
        navigationBarTitleDisplayMode(.inline)
        #else
        self
        #endif
    }

    /// Lets the Home background run under the navigation bar on iPhone.
    @ViewBuilder func transparentNavigationBar() -> some View {
        #if os(iOS)
        toolbarBackground(.hidden, for: .navigationBar)
        #else
        self
        #endif
    }
}

enum Platform {
    #if os(iOS)
    static let deviceNoun = "iPhone"
    #else
    static let deviceNoun = "Mac"
    #endif
}
