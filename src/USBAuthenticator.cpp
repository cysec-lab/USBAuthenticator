#include "USBAuthenticator.h"
#include "usb_phy_api.h"
#include "stdint.h"

using namespace arduino;

/* ----------------------CTAPInterfaceDescriptorParam---------------------- */
/** @brief One IN and one OUT endpoint */
const int CTAPInterfaceDescriptorParam::NUM_ENDPOINTS = 0x02;

/** @brief No Interface subclass */
const int CTAPInterfaceDescriptorParam::INTERFACE_SUBCLASS = 0x00;

/** @brief No Interface protocol */
const int CTAPInterfaceDescriptorParam::INTERFACE_PROTOCOL = 0x00;

/* ----------------------CTAPEndpointOneDescriptorParam---------------------- */
/** @brief Interrupt transfer */
const int CTAPEndpointOneDescriptorParam::M_ATTRIBUTES = 0x03;

/** @brief 1, OUT */
const int CTAPEndpointOneDescriptorParam::ENDPOINT_ADDRESS = 0x01;

/** @brief 64-byte packet max */
const int CTAPEndpointOneDescriptorParam::MAX_PACKET_SIZE = 64;

/** @brief Poll every 5 millisecond */
const int CTAPEndpointOneDescriptorParam::INTERVAL = 5;

/* ----------------------CTAPEndpointTwoDescriptorParam---------------------- */
/** @brief Interrupt transfer */
const int CTAPEndpointTwoDescriptorParam::M_ATTRIBUTES = 0x03;

/** @brief 1, IN */
const int CTAPEndpointTwoDescriptorParam::ENDPOINT_ADDRESS = 0x81;

/** @brief 64-byte packet max */
const int CTAPEndpointTwoDescriptorParam::MAX_PACKET_SIZE = 64;

/** @brief Poll every 5 millisecond */
const int CTAPEndpointTwoDescriptorParam::INTERVAL = 5;


/* ----------------------USBAuthenticator---------------------- */
USBAuthenticator::USBAuthenticator(bool connect, uint16_t vendor_id, uint16_t product_id, uint16_t product_release):
    USBHID(get_usb_phy(), 0, 0, vendor_id, product_id, product_release)
{
    _lock_status = 0;
}

USBAuthenticator::USBAuthenticator(USBPhy *phy, uint16_t vendor_id, uint16_t product_id, uint16_t product_release):
    USBHID(phy, 0, 0, vendor_id, product_id, product_release)
{
    _lock_status = 0;
}

USBAuthenticator::~USBAuthenticator() {
}

bool USBAuthenticator::test(HID_REPORT test) {
    _mutex.lock();

    HID_REPORT report;

    // report.data[0] = 0x01;
    
    /* test CTAPHID_INIT Command(0x06)*/
    // CID
    report.data[0] = test.data[0];
    report.data[1] = test.data[1];
    report.data[2] = test.data[2];
    report.data[3] = test.data[3];

    // CMD
    report.data[4] = 0x86;

    // BCNTH
    report.data[5] = 0x00;

    // BCNTL
    report.data[6] = 0x11;

    // DATA
    report.data[7] = test.data[7];
    report.data[8] = test.data[8];
    report.data[9] = test.data[9];
    report.data[10] = test.data[10];
    report.data[11] = test.data[11];
    report.data[12] = test.data[12];
    report.data[13] = test.data[13];
    report.data[14] = test.data[14];

    // DATA CID
    report.data[15] = 0x00;
    report.data[16] = 0x1d;
    report.data[17] = 0x00;
    report.data[18] = 0x08;

    // DATAs
    report.data[19] = 0x02;
    report.data[20] = 0x05;
    report.data[21] = 0x00;
    report.data[22] = 0x02;
    report.data[23] = 0x05;

    // zero DATA
    for (int i = 24; i < 64; i++) {
        report.data[i] = 0x00;
    }

    report.length = 64;

    if (!send(&report)) {
        _mutex.unlock();
        return false;
    }

    _mutex.unlock();
    return true;
}

const uint8_t *USBAuthenticator::report_desc() {
    static const uint8_t reportDescriptor[] = {
        USAGE_PAGE(2), 0xd1, 0xf1,      /* HID_UsagePage ( FIDO_USAGE_PAGE ) */
        USAGE(1), 0x01,                 /* HID_Usage ( FIDO_USAGE_CTAPHID ) */
        COLLECTION(1), 0x01,            /* HID_Collection ( HID_Application ) */

        USAGE(1), 0x20,                 /* HID_Usage ( FIDO_USAGE_DATA_IN ) */
        LOGICAL_MAXIMUM(1), 0x00,       /* HID_LogicalMin ( 0 ) */
        LOGICAL_MAXIMUM(2), 0xff, 0x00, /* HID_LogicalMaxS ( 0xff ) */
        REPORT_SIZE(1), 0x08,           /* HID_ReportSize ( 8 ) */
        REPORT_COUNT(1), 0x40,          /* HID_ReportCount ( HID_INPUT_REPORT_BYTES ) */
        INPUT(1), 0x02,                 /* HID_Input ( HID_DATA | HID_Absolute | HID_Variable ) */

        USAGE(1), 0x21,                 /* HID_Usage ( FIDO_USAGE_DATA_OUT) */
        LOGICAL_MINIMUM(1), 0x00,       /* HID_LogicalMin ( 0 ) */
        LOGICAL_MAXIMUM(2), 0xff, 0x00, /* HID_LogicalMaxS ( 0xff ) */
        REPORT_SIZE(1), 0x08,           /* HID_ReportSize (8) */
        REPORT_COUNT(1), 0x40,          /* HID_ReportCount ( HID_INPUT_REPORT_BYTES ) */
        OUTPUT(1), 0x02,                /* HID_Output ( HID_DATA | HID_Absolute | HID_Variable ) */

        END_COLLECTION(0),              /* HID_EndCollection */
    };
    reportLength = sizeof(reportDescriptor);
    return reportDescriptor;
}

void USBAuthenticator::report_rx() {
    assert_locked();

    HID_REPORT report;
    read_nb(&report);

    _lock_status = report.data[1] & 0x07;
}

uint8_t USBAuthenticator::lock_status() {
    return _lock_status;
}

#define DEFAULT_CONFIGURATION (1)
#define TOTAL_DESCRIPTOR_LENGTH ((1 * CONFIGURATION_DESCRIPTOR_LENGTH) \
                               + (1 * INTERFACE_DESCRIPTOR_LENGTH) \
                               + (1 * HID_DESCRIPTOR_LENGTH) \
                               + (2 * ENDPOINT_DESCRIPTOR_LENGTH))

const uint8_t *USBAuthenticator::configuration_desc(uint8_t index) {
    if (index != 0) {
        return NULL;
    }
    uint8_t configuration_descriptor_temp[] = {
        CONFIGURATION_DESCRIPTOR_LENGTH,                    // bLength
        CONFIGURATION_DESCRIPTOR,                           // bDescriptorType
        LSB(TOTAL_DESCRIPTOR_LENGTH),                       // wTotalLength (LSB)
        MSB(TOTAL_DESCRIPTOR_LENGTH),                       // wTotalLength (MSB)
        0x01,                                               // bNumInterfaces
        DEFAULT_CONFIGURATION,                              // bConfigurationValue
        0x00,                                               // iConfiguration
        C_RESERVED | C_SELF_POWERED,                        // bmAttributes
        C_POWER(0),                                         // bMaxPower

        INTERFACE_DESCRIPTOR_LENGTH,                        // bLength
        INTERFACE_DESCRIPTOR,                               // bDescriptorType
        0x00,                                               // bInterfaceNumber
        0x00,                                               // bAlternateSetting
        CTAPInterfaceDescriptorParam::NUM_ENDPOINTS,        // bNumEndpoints
        HID_CLASS,                                          // bInterfaceClass
        CTAPInterfaceDescriptorParam::INTERFACE_SUBCLASS,   // bInterfaceSubClass
        CTAPInterfaceDescriptorParam::INTERFACE_PROTOCOL,   // bInterfaceProtocol
        0x00,                                               // iInterface

        HID_DESCRIPTOR_LENGTH,                              // bLength
        HID_DESCRIPTOR,                                     // bDescriptorType
        LSB(HID_VERSION_1_11),                              // bcdHID (LSB)
        MSB(HID_VERSION_1_11),                              // bdcHID (MSB)
        0x00,                                               // bCountryCode
        0x01,                                               // bNumDescriptors
        REPORT_DESCRIPTOR,                                  // bDescriptorType,
        (uint8_t)(LSB(report_desc_length())),               // wDescriptorLength (LSB)
        (uint8_t)(MSB(report_desc_length())),               // wDescriptorLength (MSB)

        ENDPOINT_DESCRIPTOR_LENGTH,                         // bLength
        ENDPOINT_DESCRIPTOR,                                // bDescriptorType
        CTAPEndpointOneDescriptorParam::ENDPOINT_ADDRESS,   // bEndpointAddress
        CTAPEndpointOneDescriptorParam::M_ATTRIBUTES,       // bmAttributes
        LSB(CTAPEndpointOneDescriptorParam::MAX_PACKET_SIZE), // wMaxPacketSize (LSB)
        MSB(CTAPEndpointOneDescriptorParam::MAX_PACKET_SIZE), // wMaxPacketSize (MSB)
        CTAPEndpointOneDescriptorParam::INTERVAL,           // bInterval (milliseconds)

        ENDPOINT_DESCRIPTOR_LENGTH,                         // bLength,
        ENDPOINT_DESCRIPTOR,                                // bDescriptorType
        CTAPEndpointTwoDescriptorParam::ENDPOINT_ADDRESS,   // bEndpointAddress
        CTAPEndpointTwoDescriptorParam::M_ATTRIBUTES,       // bmAttributes
        LSB(CTAPEndpointTwoDescriptorParam::MAX_PACKET_SIZE), // wMaxPacketSize (LSB)
        MSB(CTAPEndpointTwoDescriptorParam::MAX_PACKET_SIZE), // wMaxPacketSize (MSB)
        CTAPEndpointTwoDescriptorParam::INTERVAL,           // bInterval (milliseconds)
    };
    MBED_ASSERT(sizeof(configuration_descriptor_temp) == sizeof(_configuration_descriptor));
    memcpy(_configuration_descriptor, configuration_descriptor_temp, sizeof(_configuration_descriptor));
    return _configuration_descriptor;
}

/**
 * @brief ????????????????????????????????????
 * 
 * @param report - ???????????????HID_REPORT
 */
void USBAuthenticator::parseRequest(HID_REPORT report) {
    this->req = new Request;
    /* CTAP Initialization Packet?????? */
    // Channel Identifier?????????
    memcpy(this->req->channelID, report.data, 4);
    // Command Identifier?????????
    this->req->command = (unsigned int)report.data[4];
    // BCNTH?????????
    this->req->BCNTH = (unsigned int)report.data[5];
    // BCNTL?????????
    this->req->BCNTL = (unsigned int)report.data[6] * 16 * 16;
    this->req->BCNTL = this->req->BCNTL + (unsigned int)report.data[7];
    this->writeCount = this->req->BCNTL;

    if (this->req->BCNTL > 55) { /* ??????????????????????????????????????? */
        this->continuationFlag = true;
    }

    /* authenticatorAPI?????????????????? */
    // Command Value?????????
    this->req->data.commandValue = (unsigned int)report.data[8];
    // commandParameter?????????
    this->req->data.commandParameter = new uint8_t[this->req->BCNTL-1];
    for (int i=0; i<64-9; i++) {
        if (!(this->writeCount > 0)) {
            break;
        }
        this->req->data.commandParameter[i] = report.data[9+i];
        this->req->dataSize++;
        this->writeCount--;
    }
    // this->req->SerialDebug();
}

/**
 * @brief ????????????????????????????????????
 * 
 * @param report - ???????????????HID_REPORT
 */
void USBAuthenticator::parseContinuationPacket(HID_REPORT report) {
    this->continuation = new ContinuationPacket;
    // Channel Identifier?????????
    memcpy(this->continuation->channelID, report.data, 4);
    // Packet Sequence?????????
    this->continuation->sequence = (unsigned int)report.data[4];
    // Payload data?????????
    this->continuation->data = new uint8_t[64-5];
    for (int i=0; i<64-5; i++) {
        if (!(this->writeCount > 0)) {
            this->continuationFlag = false;
            break;
        }
        this->continuation->data[i] = report.data[5+i];
        this->continuation->dataSize++;
        this->writeCount--;
    }
    // this->continuation->SerialDebug();
    this->connectRequestData();

    /* ?????????????????????????????? */
    delete this->continuation->data;
    delete this->continuation;
}

/**
 * @brief Request???Data??????????????????????????????????????????????????????
 */
void USBAuthenticator::connectRequestData() {
    for (int i=0; i<this->continuation->dataSize; i++) {
        this->req->data.commandParameter[this->req->dataSize] = this->continuation->data[i];
        this->req->dataSize++;
    }
}

/**
 * @brief CTAP???????????????
 */
void USBAuthenticator::operate() {
    try {
        operateCTAPCommand();
    } catch (std::exception& e) {
        // TODO:?????????????????????????????????????????????catch??????????????????
        Serial.println(e.what());
    }
    sendResponse();
    delete this->req->data.commandParameter;
    delete this->req;
}

/**
 * @brief CTAP???????????????????????????????????????????????????
 */
void USBAuthenticator::operateCTAPCommand() {
    switch (this->req->command) { /* Command????????????????????????????????? */
        case CTAPHID_MSG:
            operateMSGCommand(); break;
        case CTAPHID_CBOR:
            operateCBORCommand(); break;
        case CTAPHID_INIT:
            operateINITCommand(); break;
        case CTAPHID_PING:
            operatePINGCommand(); break;
        case CTAPHID_CANCEL:
            operateCANCELCommand(); break;
        case CTAPHID_ERROR:
            operateERRORCommand(); break;
        case CTAPHID_KEEPALIVE:
            operateKEEPALIVECommand(); break;
        default: /* Command???????????????????????? */
            throw implement_error("Not implement CTAP Command."); break;
    }
}

/**
 * @brief MSG???????????????????????????
 */
void USBAuthenticator::operateMSGCommand() {
    throw implement_error("Not implement MSG Command.");
}

/**
 * @brief CBOR???????????????????????????
 */
void USBAuthenticator::operateCBORCommand() {
    // throw implement_error("Not implement CBOR Command.");
    if (checkHasParameters(this->req->data.commandValue)) {
        this->authAPI = new AuthenticatorAPI(this->req->data.commandValue, this->req->data.commandParameter, this->req->BCNTL);
    } else {
        this->authAPI = new AuthenticatorAPI(this->req->data.commandValue);
    }

    /* ?????????????????? */
    if (this->authAPI->getCommand() == AuthenticatorAPICommandParam::COMMAND_GETASSERTION) {
        this->authAPI->setTPK(this->getTPK());
        this->authAPI->setAPK(this->getAPK());
        this->authAPI->setSKA(this->getSKA());
    }

    try {
        this->response = this->authAPI->operateCommand();
        this->response->ResponseSerialDebug();
    } catch (implement_error& e) {
        throw implement_error(e.what());
    }

    /* ?????????????????? */
    if (this->authAPI->getCommand() == AuthenticatorAPICommandParam::COMMAND_MAKECREDENTIAL) {
        setTPK(this->authAPI->getTPK());
        setAPK(this->authAPI->getAPK());
        setSKA(this->authAPI->getSKA());
    }
}

/**
 * @brief INIT???????????????????????????
 */
void USBAuthenticator::operateINITCommand() {
    throw implement_error("Not implement INIT Command.");
}

/**
 * @brief PING???????????????????????????
 */
void USBAuthenticator::operatePINGCommand() {
    throw implement_error("Not implement PING Command.");
}

/**
 * @brief CANCEL???????????????????????????
 */
void USBAuthenticator::operateCANCELCommand() {
    throw implement_error("Not implement CANCEL Command.");
}

/**
 * @brief ERROR???????????????????????????
 */
void USBAuthenticator::operateERRORCommand() {
    throw implement_error("Not implement ERROR Command.");
}

/**
 * @brief KEEPALIVE???????????????????????????
 */ 
void USBAuthenticator::operateKEEPALIVECommand() {
    throw implement_error("Not implement KEEPALIVE Command.");
}

/**
 * @brief ??????????????????????????????????????????
 */
void USBAuthenticator::sendResponse() {
    HID_REPORT report;
    size_t reportLength = 0;
    size_t responseDataLength = 0;
    size_t continuationCount = 0;
    // Response????????????55???????????????????????????????????????
    if (this->response->length > 55) { // ?????????????????????????????????????????????
        continuationCount = ((this->response->length - 55) / 59) + 1;
    }

    /* Initialize Packet????????? */
    // channel identifier?????????(Request?????????)
    for (int i=0; i<4; i++) {
        report.data[reportLength] = this->req->channelID[i];
        reportLength++;
    }

    // command Identifier?????????(Request?????????)
    report.data[reportLength] = this->req->command;
    reportLength++;

    // BCNTH?????????
    report.data[reportLength] = 0x00;
    reportLength++;

    // BCNTL?????????
    report.data[reportLength] = (uint8_t)(this->response->length/256);
    reportLength++;
    report.data[reportLength] = (uint8_t)(this->response->length%256);
    reportLength++;

    // Data?????????
    report.data[reportLength] = this->response->status;
    reportLength++;
    for (int i=0; i<55; i++) {
        if (responseDataLength > response->length) { /* ?????????????????? */
            report.data[reportLength] = 0x00;
            reportLength++;
        } else { /* ????????????????????? */
            report.data[reportLength] = this->response->responseData[responseDataLength];
            reportLength++;
            responseDataLength++;
        }
    }
    // ???????????????
    report.length = 64;
    // ??????
    if (!send(&report)) {
        _mutex.unlock();
    }

    /* Continuation Packet????????? */
    for (size_t i=0; i<continuationCount; i++) {
        HID_REPORT continuationReport;
        size_t continuationLength = 0;
        // channel identifier?????????(Request?????????)
        for (int j=0; j<4; j++) {
            continuationReport.data[continuationLength] = this->req->channelID[j];
            continuationLength++;
        }

        // Packet Sequence?????????
        continuationReport.data[continuationLength] = 0x80 + i;
        continuationLength++;

        // Data?????????
        for (int j=0; j<59; j++) { /* ?????????????????? */
            if (responseDataLength > response->length) {
                continuationReport.data[continuationLength] = 0x00;
                continuationLength++;
            } else { /* ????????????????????? */
                continuationReport.data[continuationLength] = this->response->responseData[responseDataLength];
                continuationLength++;
                responseDataLength++;
            }
        }
        // ???????????????
        continuationReport.length = 64;
        // ??????
        if (!send(&continuationReport)) {
            _mutex.unlock();
        }
    }

    Serial.println("Packet end.");
}

bool USBAuthenticator::getWriteFlag() {
    return this->writeFlag;
}

bool USBAuthenticator::getContinuationFlag() {
    return this->continuationFlag;
}

AuthenticatorAPI *USBAuthenticator::getAuthAPI() {
    return this->authAPI;
}

TPK *USBAuthenticator::getTPK() {
    return this->tpk;
}

APK *USBAuthenticator::getAPK() {
    return this->apk;
}

SKA *USBAuthenticator::getSKA() {
    return this->ska;
}

void USBAuthenticator::setWriteFlag(bool writeFlag) {
    this->writeFlag = writeFlag;
}

void USBAuthenticator::setContinuationFlag(bool continuationFlag) {
    this->continuationFlag = continuationFlag;
}

void USBAuthenticator::setTPK(TPK *tpk) {
    this->tpk = tpk;
}

void USBAuthenticator::setAPK(APK *apk) {
    this->apk = apk;
}

void USBAuthenticator::setSKA(SKA *ska) {
    this->ska = ska;
}