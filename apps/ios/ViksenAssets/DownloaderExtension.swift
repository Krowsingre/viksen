import BackgroundAssets
import ExtensionFoundation
import StoreKit

/// Downloads Dimensions (Apple-hosted asset packs) for Viksen, including while the
/// app is not running. The system implementation does all the work; every Dimension
/// pack is on-demand, so nothing downloads until the listener asks for it.
@main
struct DownloaderExtension: StoreDownloaderExtension {}
