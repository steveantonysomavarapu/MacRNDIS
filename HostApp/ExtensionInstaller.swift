import Foundation
import SystemExtensions

final class ExtensionInstaller: NSObject, OSSystemExtensionRequestDelegate {
  private let extensionIdentifier = "com.example.macrndis.driver"

  func install() {
    let request = OSSystemExtensionRequest.activationRequest(
      forExtensionWithIdentifier: extensionIdentifier,
      queue: .main
    )
    request.delegate = self
    OSSystemExtensionManager.shared.submitRequest(request)
  }

  func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
    NSLog("System extension request finished: %d", result.rawValue)
  }

  func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
    NSLog("System extension request failed: %@", String(describing: error))
  }

  func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
    NSLog("System extension needs user approval in System Settings")
  }

  func request(
    _ request: OSSystemExtensionRequest,
    actionForReplacingExtension existing: OSSystemExtensionProperties,
    withExtension extensionProps: OSSystemExtensionProperties
  ) -> OSSystemExtensionRequest.ReplacementAction {
    NSLog("Replacing existing system extension: %@", existing.bundleIdentifier)
    return .replace
  }
}

let installer = ExtensionInstaller()
installer.install()
RunLoop.main.run()
