#include "BluetoothPairingAgent.h"
#include "ConnectivityBackendPlugin.h"
#include "WifiController.h"

#include <qqml.h>

void ConnectivityBackendPlugin::registerTypes(const char *uri) {
    // Types are now automatically registered via QML_ELEMENT/QML_SINGLETON in headers
    Q_UNUSED(uri);
}
