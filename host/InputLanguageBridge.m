#import <Carbon/Carbon.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

static const char *kSerialPattern = "/dev/cu.usbmodem*";
static char gLastSourceID[256] = {};
static NSString *const kLanguageSyncServiceUUID =
    @"7A780001-6D9F-4D4E-9E37-9A6E41F5A101";
static NSString *const kLanguageSyncCharacteristicUUID =
    @"7A780002-6D9F-4D4E-9E37-9A6E41F5A101";

static CFTypeRef sourceProperty(TISInputSourceRef source, CFStringRef key) {
    return TISGetInputSourceProperty(source, key);
}

static bool copyStringProperty(TISInputSourceRef source, CFStringRef key,
                               char *buffer, size_t bufferSize) {
    CFStringRef value = (CFStringRef)sourceProperty(source, key);
    if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID()) {
        return false;
    }
    return CFStringGetCString(value, buffer, bufferSize,
                              kCFStringEncodingUTF8);
}

static CFArrayRef copySelectableKeyboardSources(void) {
    const void *keys[] = {
        kTISPropertyInputSourceCategory,
        kTISPropertyInputSourceIsEnabled,
        kTISPropertyInputSourceIsSelectCapable,
    };
    const void *values[] = {
        kTISCategoryKeyboardInputSource,
        kCFBooleanTrue,
        kCFBooleanTrue,
    };
    CFDictionaryRef filter = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef sources = TISCreateInputSourceList(filter, false);
    CFRelease(filter);
    return sources;
}

static void languageCode(TISInputSourceRef source, char code[3]) {
    char id[256] = {};
    char name[256] = {};
    copyStringProperty(source, kTISPropertyInputSourceID, id, sizeof(id));
    copyStringProperty(source, kTISPropertyLocalizedName, name, sizeof(name));

    if (strstr(id, "Russian") != NULL || strstr(name, "Russian") != NULL) {
        memcpy(code, "RU", 3);
        return;
    }
    if (strstr(id, "ABC") != NULL || strstr(id, "US") != NULL ||
        strstr(name, "ABC") != NULL || strstr(name, "U.S.") != NULL) {
        memcpy(code, "EN", 3);
        return;
    }

    CFArrayRef languages =
        (CFArrayRef)sourceProperty(source, kTISPropertyInputSourceLanguages);
    if (languages != NULL && CFGetTypeID(languages) == CFArrayGetTypeID() &&
        CFArrayGetCount(languages) > 0) {
        CFStringRef language =
            (CFStringRef)CFArrayGetValueAtIndex(languages, 0);
        char tag[16] = {};
        if (CFStringGetCString(language, tag, sizeof(tag),
                               kCFStringEncodingUTF8) && strlen(tag) >= 2) {
            code[0] = (char)toupper((unsigned char)tag[0]);
            code[1] = (char)toupper((unsigned char)tag[1]);
            code[2] = '\0';
            return;
        }
    }

    memcpy(code, "??", 3);
}

@interface BluetoothLanguageBridge
    : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
@property(nonatomic, strong) CBCentralManager *manager;
@property(nonatomic, strong) CBPeripheral *peripheral;
@property(nonatomic, strong) CBCharacteristic *languageCharacteristic;
- (void)start;
- (void)sendLanguageCode:(const char *)code sourceID:(const char *)sourceID;
@end

@implementation BluetoothLanguageBridge

- (void)start {
    self.manager = [[CBCentralManager alloc] initWithDelegate:self
                                                        queue:nil];
}

- (void)findDevice {
    CBUUID *service = [CBUUID UUIDWithString:kLanguageSyncServiceUUID];
    NSArray<CBPeripheral *> *connected =
        [self.manager retrieveConnectedPeripheralsWithServices:@[ service ]];
    if (connected.count > 0) {
        [self connectDevice:connected.firstObject];
        return;
    }
    [self.manager scanForPeripheralsWithServices:@[ service ]
                                         options:@{
                                             CBCentralManagerScanOptionAllowDuplicatesKey : @NO
                                         }];
    fprintf(stderr, "bluetooth: scanning for Round Language Switch\n");
}

- (void)connectDevice:(CBPeripheral *)peripheral {
    if (self.peripheral != nil) {
        return;
    }
    self.peripheral = peripheral;
    self.peripheral.delegate = self;
    [self.manager stopScan];
    [self.manager connectPeripheral:peripheral options:nil];
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    if (central.state == CBManagerStatePoweredOn) {
        [self findDevice];
    } else {
        self.languageCharacteristic = nil;
        self.peripheral = nil;
        fprintf(stderr, "bluetooth: unavailable (state=%ld)\n",
                (long)central.state);
    }
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
    (void)advertisementData;
    (void)RSSI;
    [self connectDevice:peripheral];
}

- (void)centralManager:(CBCentralManager *)central
    didConnectPeripheral:(CBPeripheral *)peripheral {
    (void)central;
    fprintf(stderr, "bluetooth connected: %s\n",
            peripheral.name.UTF8String ?: "Round Language Switch");
    [peripheral discoverServices:@[
        [CBUUID UUIDWithString:kLanguageSyncServiceUUID]
    ]];
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(NSError *)error {
    (void)central;
    (void)peripheral;
    fprintf(stderr, "bluetooth connect failed: %s\n",
            error.localizedDescription.UTF8String ?: "unknown error");
    self.languageCharacteristic = nil;
    self.peripheral = nil;
    [self findDevice];
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                       error:(NSError *)error {
    (void)central;
    (void)peripheral;
    fprintf(stderr, "bluetooth disconnected%s%s\n",
            error == nil ? "" : ": ",
            error == nil ? "" : error.localizedDescription.UTF8String);
    self.languageCharacteristic = nil;
    self.peripheral = nil;
    [self findDevice];
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverServices:(NSError *)error {
    if (error != nil) {
        fprintf(stderr, "bluetooth service discovery failed: %s\n",
                error.localizedDescription.UTF8String);
        return;
    }
    CBUUID *target = [CBUUID UUIDWithString:kLanguageSyncServiceUUID];
    for (CBService *service in peripheral.services) {
        if ([service.UUID isEqual:target]) {
            [peripheral discoverCharacteristics:@[
                [CBUUID UUIDWithString:kLanguageSyncCharacteristicUUID]
            ] forService:service];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(NSError *)error {
    (void)service;
    if (error != nil) {
        fprintf(stderr, "bluetooth characteristic discovery failed: %s\n",
                error.localizedDescription.UTF8String);
        return;
    }
    CBUUID *target =
        [CBUUID UUIDWithString:kLanguageSyncCharacteristicUUID];
    for (CBCharacteristic *characteristic in service.characteristics) {
        if ([characteristic.UUID isEqual:target]) {
            self.languageCharacteristic = characteristic;
            fprintf(stderr, "bluetooth language sync ready\n");

            TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
            if (current != NULL) {
                char code[3] = {};
                char sourceID[256] = {};
                languageCode(current, code);
                copyStringProperty(current, kTISPropertyInputSourceID,
                                   sourceID, sizeof(sourceID));
                [self sendLanguageCode:code sourceID:sourceID];
                CFRelease(current);
            }
            break;
        }
    }
    (void)peripheral;
}

- (void)sendLanguageCode:(const char *)code sourceID:(const char *)sourceID {
    if (self.peripheral == nil || self.languageCharacteristic == nil ||
        code == NULL || strlen(code) < 2) {
        return;
    }
    char command[16] = {};
    int length = snprintf(command, sizeof(command), "LANG %.2s\n", code);
    if (length <= 0) {
        return;
    }
    NSData *data = [NSData dataWithBytes:command length:(NSUInteger)length];
    CBCharacteristicWriteType type =
        (self.languageCharacteristic.properties & CBCharacteristicPropertyWrite)
            ? CBCharacteristicWriteWithResponse
            : CBCharacteristicWriteWithoutResponse;
    [self.peripheral writeValue:data
              forCharacteristic:self.languageCharacteristic
                           type:type];
    fprintf(stderr, "bluetooth layout: %.2s (%s)\n", code,
            sourceID == NULL ? "" : sourceID);
}

@end

static BluetoothLanguageBridge *gBluetoothBridge = nil;

static bool copyCurrentSourceID(char *buffer, size_t bufferSize) {
    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    if (current == NULL) {
        return false;
    }
    bool copied = copyStringProperty(current, kTISPropertyInputSourceID,
                                     buffer, bufferSize);
    CFRelease(current);
    return copied;
}

static bool writeAll(int fd, const char *bytes, size_t length) {
    while (length > 0) {
        ssize_t written = write(fd, bytes, length);
        if (written > 0) {
            bytes += written;
            length -= (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

static bool sendLanguageForSource(int fd, TISInputSourceRef source) {
    char code[3] = {};
    char sourceID[256] = {};
    languageCode(source, code);
    copyStringProperty(source, kTISPropertyInputSourceID,
                       sourceID, sizeof(sourceID));

    char command[32] = {};
    int length = snprintf(command, sizeof(command), "LANG %s\n", code);
    bool sent = fd < 0 ||
                (length > 0 && writeAll(fd, command, (size_t)length));
    [gBluetoothBridge sendLanguageCode:code sourceID:sourceID];
    fprintf(stderr, "layout: %s (%s)%s\n", code, sourceID,
            sent ? "" : " [send failed]");
    return sent;
}

static bool sendCurrentLanguage(int fd) {
    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    if (current == NULL) {
        return false;
    }
    bool sent = sendLanguageForSource(fd, current);
    CFRelease(current);
    return sent;
}

static bool selectNextInputSource(int fd) {
    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    CFArrayRef sources = copySelectableKeyboardSources();
    if (current == NULL || sources == NULL || CFArrayGetCount(sources) < 2) {
        if (current != NULL) CFRelease(current);
        if (sources != NULL) CFRelease(sources);
        return false;
    }

    CFStringRef currentID =
        (CFStringRef)sourceProperty(current, kTISPropertyInputSourceID);
    CFIndex sourceCount = CFArrayGetCount(sources);
    CFIndex currentIndex = -1;
    for (CFIndex index = 0; index < sourceCount; ++index) {
        TISInputSourceRef source =
            (TISInputSourceRef)CFArrayGetValueAtIndex(sources, index);
        CFStringRef sourceID =
            (CFStringRef)sourceProperty(source, kTISPropertyInputSourceID);
        if (currentID != NULL && sourceID != NULL &&
            CFEqual(currentID, sourceID)) {
            currentIndex = index;
            break;
        }
    }

    CFIndex nextIndex = currentIndex < 0 ? 0 : (currentIndex + 1) % sourceCount;
    TISInputSourceRef next =
        (TISInputSourceRef)CFArrayGetValueAtIndex(sources, nextIndex);
    OSStatus status = TISSelectInputSource(next);

    char name[256] = {};
    copyStringProperty(next, kTISPropertyLocalizedName, name, sizeof(name));
    fprintf(stderr, "switch: %s (status=%d)\n", name, (int)status);

    bool sent = true;
    if (status == noErr) {
        copyStringProperty(next, kTISPropertyInputSourceID,
                           gLastSourceID, sizeof(gLastSourceID));
        // TISSelectInputSource is synchronous. Send the source we selected
        // immediately instead of sleeping and querying macOS again.
        if (fd >= 0) {
            sent = sendLanguageForSource(fd, next);
        }
    }

    CFRelease(sources);
    CFRelease(current);
    return status == noErr && sent;
}

static int openSerialDevice(char path[PATH_MAX]) {
    glob_t matches = {};
    if (glob(kSerialPattern, 0, NULL, &matches) != 0 ||
        matches.gl_pathc == 0) {
        globfree(&matches);
        return -1;
    }

    int fd = -1;
    for (size_t index = 0; index < matches.gl_pathc; ++index) {
        fd = open(matches.gl_pathv[index], O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0) {
            strlcpy(path, matches.gl_pathv[index], PATH_MAX);
            break;
        }
    }
    globfree(&matches);
    if (fd < 0) {
        return -1;
    }

    struct termios options = {};
    if (tcgetattr(fd, &options) == 0) {
        cfmakeraw(&options);
        cfsetspeed(&options, B115200);
        options.c_cflag |= CLOCAL | CREAD;
        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 1;
        tcsetattr(fd, TCSANOW, &options);
    }
    return fd;
}

static bool handleDeviceLine(const char *line, int fd) {
    if (strncmp(line, "READY", 5) == 0 || strcmp(line, "TAP") == 0 ||
        strncmp(line, "DISPLAYED", 9) == 0 ||
        strncmp(line, "ERROR", 5) == 0) {
        fprintf(stderr, "device: %s\n", line);
    }
    if (strcmp(line, "TAP") == 0) {
        return selectNextInputSource(fd);
    } else if (strncmp(line, "READY", 5) == 0) {
        return sendCurrentLanguage(fd);
    }
    return true;
}

static bool readDevice(int fd) {
    static char line[256] = {};
    static size_t lineLength = 0;
    char input[256];

    while (true) {
        ssize_t count = read(fd, input, sizeof(input));
        if (count > 0) {
            for (ssize_t index = 0; index < count; ++index) {
                char value = input[index];
                if (value == '\n') {
                    line[lineLength] = '\0';
                    if (!handleDeviceLine(line, fd)) {
                        return false;
                    }
                    lineLength = 0;
                } else if (value != '\r' && lineLength < sizeof(line) - 1) {
                    line[lineLength++] = value;
                }
            }
            continue;
        }
        if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
            errno != EINTR) {
            return false;
        }
        return true;
    }
}

int main(int argc, const char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stderr, NULL, _IOLBF, 0);

    if (argc == 2 && strcmp(argv[1], "--switch-once") == 0) {
        return selectNextInputSource(-1) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    gBluetoothBridge = [[BluetoothLanguageBridge alloc] init];
    [gBluetoothBridge start];

    int serialFD = -1;
    char serialPath[PATH_MAX] = {};
    unsigned int reconnectTicks = 0;
    unsigned int sourcePollTicks = 0;

    fprintf(stderr, "input-language bridge started\n");
    while (true) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);

        if (serialFD < 0) {
            if (reconnectTicks == 0) {
                serialFD = openSerialDevice(serialPath);
                if (serialFD >= 0) {
                    fprintf(stderr, "serial connected: %s\n", serialPath);
                    usleep(300000);
                    if (!sendCurrentLanguage(serialFD)) {
                        close(serialFD);
                        serialFD = -1;
                    }
                }
            }
            reconnectTicks = (reconnectTicks + 1) % 20;
        } else if (!readDevice(serialFD)) {
            fprintf(stderr, "serial disconnected\n");
            close(serialFD);
            serialFD = -1;
            reconnectTicks = 0;
        }

        // Keep USB tap latency near 10 ms while checking external keyboard
        // layout changes at the original 50 ms cadence.
        if (sourcePollTicks == 0) {
            char currentSourceID[256] = {};
            if (copyCurrentSourceID(currentSourceID,
                                    sizeof(currentSourceID)) &&
                strcmp(currentSourceID, gLastSourceID) != 0) {
                strlcpy(gLastSourceID, currentSourceID,
                        sizeof(gLastSourceID));
                if (!sendCurrentLanguage(serialFD) && serialFD >= 0) {
                    close(serialFD);
                    serialFD = -1;
                    reconnectTicks = 0;
                }
            }
        }
        sourcePollTicks = (sourcePollTicks + 1) % 5;

        usleep(10000);
    }
}
