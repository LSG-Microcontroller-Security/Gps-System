#include "mf_repository_SimRepository.h"
#include <string.h>
#include "Arduino.h"
// Comando AT per chiamare: ATD<numero>;\r
// Comandi AT principali
#define AT_SMS_HEADER             "+CMGR:"
#define AT_SET_SMS_TEXT_MODE      "AT+CMGF=1"
#define AT_SET_SMS_PDU_MODE       "AT+CMGF=0"
#define AT_SET_SMS_SHOW_IND       "AT+CNMI=2,2,0,0,0"
#define AT_SET_SMS_CHARSET        "AT+CSCS=\"GSM\""
#define ATD_COMMAND               "ATD+"
#define HANGUP_COMMAND            "ATH"
#define AT_SEND_SMS_PREFIX        "AT+CMGS=\""
#define LITERAL_OK                "OK"
#define LITERAL_CTRL_Z            ((char)26)
#define LITERAL_RETURN            "\r\n"
#define AT_READ_SMS_PREFIX   "AT+CMGR="
#define AT_DELETE_ALL_SMS "AT+CMGD=1,4"
#define AT_SET_SMS_MEMORY      "AT+CPMS=\"SM\""
#define AT_GET_SMS_STORAGE_STATUS "AT+CPMS?"
#define AT_SMS_STORAGE_HEADER "+CPMS:"
#define AT_DELETE_SMS_PREFIX "AT+CMGD="



SimRepository::SimRepository(AvrMicroRepository& avrMicroRepository, uint8_t powerPin, unsigned long baudRate)
    : avrMicroRepository(&avrMicroRepository), powerPin(powerPin), baudRate(baudRate) {
	avrMicroRepository.begin(baudRate); // Inizializza la comunicazione seriale
}

bool SimRepository::call(const char* number) {
    // Precondizione: modulo acceso e pronto, SIM inserita e unlockata
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(ATD_COMMAND);      // "ATD"
    avrMicroRepository->print(number);           // il numero
    avrMicroRepository->print(";");              // punto e virgola per chiamata vocale
    avrMicroRepository->print("\r");             // return per invio comando

    // Attendi risposta "OK" o "NO CARRIER" ecc, gestisci qui se vuoi
    unsigned long start = avrMicroRepository->get_millis();
    while (avrMicroRepository->get_millis() - start < 5000) {
        if (avrMicroRepository->available()) {
            char c = avrMicroRepository->read();
            // Qui puoi implementare una logica per "OK", "NO CARRIER", ecc.
            // Per ora semplifico: se arriva un carattere, assumi che qualcosa è successo.
            return true;
        }
    }
    // Timeout: nessuna risposta dal modulo
    return false;
}

void SimRepository::hangUp() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(HANGUP_COMMAND);   // "ATH\r"
	avrMicroRepository->print(LITERAL_RETURN);   // return per invio comando
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 2000);
}

void SimRepository::setSmsTextMode() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_SET_SMS_TEXT_MODE);
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 2000);
}

void SimRepository::setSmsPduMode() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_SET_SMS_PDU_MODE);
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 2000);
}

bool SimRepository::sendSms(const char* number, const char* message) {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_SEND_SMS_PREFIX);  // "AT+CMGS=\""
    avrMicroRepository->print(number);
    avrMicroRepository->print("\"");                // <--- CHIUSURA VIRGOLETTE MANCANTE!
    avrMicroRepository->print(LITERAL_RETURN);

    // Attendi prompt '>'
    unsigned long start = avrMicroRepository->get_millis();
    bool gotPrompt = false;
    while (avrMicroRepository->get_millis() - start < 3000) {
        if (avrMicroRepository->available()) {
            char c = avrMicroRepository->read();
            if (c == '>') {
                gotPrompt = true;
                break;
            }
        }
    }
    if (!gotPrompt) return false;

    avrMicroRepository->print(message);
    avrMicroRepository->print(LITERAL_CTRL_Z);
    avrMicroRepository->delay(1000);  // opzionale

    return true;
}

void SimRepository::initSmsReception() {
    // 1. Seleziona la memoria SMS sulla SIM
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_SET_SMS_MEMORY);       // "AT+CPMS=\"SM\""
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 5000);

    // 2. Imposta la modalità testo
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_SET_SMS_TEXT_MODE);    // "AT+CMGF=1"
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 5000);

    // 3. Cancella tutti gli SMS dalla memoria selezionata
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_DELETE_ALL_SMS);       // "AT+CMGD=1,4"
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 5000);

    // 4. Imposta la notifica E la memorizzazione degli SMS ricevuti
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print("AT+CNMI=1,1,0,0,0");     // **Questa è la riga cruciale**
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 5000);

    // (Opzionale) Imposta charset GSM
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_SET_SMS_CHARSET);      // "AT+CSCS=\"GSM\""
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 5000);
	this->isSmsReceiveInitialized = true; // Flag per indicare che la ricezione SMS è stata inizializzata 
}

bool SimRepository::readSms(uint8_t index, char* message) {
    if(this->isSmsReceiveInitialized == false) {
		initSmsReception();
	}
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_READ_SMS_PREFIX);
    avrMicroRepository->print(index);
    avrMicroRepository->print(LITERAL_RETURN);
    bool foundHeader = false;
    bool foundMsg = false;
    int msgPos = 0;
    int crlfCount = 0;  // Conta i CR/LF per trovare inizio testo
    unsigned long start = avrMicroRepository->get_millis();
    while (avrMicroRepository->get_millis() - start < 2500 && msgPos < avrMicroRepository->getSerialBufferSize()) {
        if (avrMicroRepository->available()) {
            char c = avrMicroRepository->read();
            // Cerca la riga header
            if (!foundHeader) {
                static char lastChars[7] = { 0 };
                // Shift buffer
                memmove(lastChars, lastChars + 1, 6);
                lastChars[6] = c;
                // Se negli ultimi 6 char c'è "+CMGR:"
                if (strncmp(lastChars, AT_SMS_HEADER, 6) == 0) {
                    foundHeader = true;
                    crlfCount = 0;
                }
            }
            else if (!foundMsg) {
                // Conta \n per saltare l’header, cerca la prima riga utile
                if (c == '\n') crlfCount++;
                // Dopo il primo CRLF, inizia a copiare il testo
                if (crlfCount == 1 && c != '\r' && c != '\n') {
                    message[msgPos++] = c;
                    foundMsg = true;
                }
            }
            else {
                // Continua a copiare caratteri del messaggio fino a CR/LF
                if (c == '\r' || c == '\n') {
                    break;
                }
                else if (msgPos < avrMicroRepository->getSerialBufferSize()) {
                    message[msgPos++] = c;
                }
            }
        }
    }
    message[msgPos] = '\0';
    return foundMsg && (msgPos > 0);
}

void SimRepository::deleteAllSms() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_DELETE_ALL_SMS);
    avrMicroRepository->print(LITERAL_RETURN);
    SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 3000);
}

int SimRepository::getSmsCount() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_GET_SMS_STORAGE_STATUS);
    avrMicroRepository->print(LITERAL_RETURN);

    char buffer[64];
    int bufPos = 0;
    unsigned long start = avrMicroRepository->get_millis();

    // Attendi una risposta (max 2 secondi)
    while (avrMicroRepository->get_millis() - start < 2000 && bufPos < (int)sizeof(buffer) - 1) {
        if (avrMicroRepository->available()) {
            char c = avrMicroRepository->read();
			//Serial.print(c); // Debug: stampa il carattere ricevuto
            buffer[bufPos++] = c;
        }
    }
    buffer[bufPos] = '\0';

    // Cerca la riga che inizia con "+CPMS:"
    char* ptr = strstr(buffer, AT_SMS_STORAGE_HEADER);
    if (!ptr) return -1; // non trovato

    // Esempio di risposta: +CPMS: "SM",2,30,...
    ptr = strchr(ptr, ','); // primo ','
    if (!ptr) return -1;
    int num_used = atoi(ptr + 1); // il primo numero dopo il ','

    return num_used;
}

bool SimRepository::deleteSmsAt(uint8_t index) {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print(AT_DELETE_SMS_PREFIX);
    avrMicroRepository->print(index);
    avrMicroRepository->print(LITERAL_RETURN);

    // Attendi conferma "OK" o timeout
    return SerialUtils::wait_for_pattern(*this->avrMicroRepository, LITERAL_OK, 2000);
}

uint8_t SimRepository::findSmsWithContent(const char* pattern) {
    if (this->isSmsReceiveInitialized == false) {
        initSmsReception();
    }
	uint8_t smsCount = getSmsCount();
    char buffer[avrMicroRepository->getSerialBufferSize()];
    for (uint8_t i = 1; i <= smsCount; ++i) {
        if (readSms(i, buffer)) {
            if (strcmp(buffer, pattern) == 0) {
                return i; // trovato esattamente
            }
        }
    }
    return 0; // non trovato
}
bool SimRepository::isRegisteredToNetwork() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print("AT+CREG?");
    avrMicroRepository->print(LITERAL_RETURN);

    char buffer[32];
    int pos = 0;
    unsigned long start = avrMicroRepository->get_millis();

    while (avrMicroRepository->get_millis() - start < 2000 && pos < sizeof(buffer) - 1) {
        if (avrMicroRepository->available()) {
            char c = avrMicroRepository->read();
            buffer[pos++] = c;
        }
    }
    buffer[pos] = '\0';

    // Cerca la risposta "+CREG: 0,1" o "+CREG: 0,5"
    char* creg = strstr(buffer, "+CREG:");
    if (creg != nullptr) {
        int n, stat;
        if (sscanf(creg, "+CREG: %d,%d", &n, &stat) == 2) {
            return (stat == 1 || stat == 5); // 1: registrato, 5: roaming
        }
    }
    return false; // Non registrato
}
int SimRepository::getSignalStrength() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print("AT+CSQ");
    avrMicroRepository->print(LITERAL_RETURN);
    char buffer[32];
    int pos = 0;
    unsigned long start = avrMicroRepository->get_millis();
    while (avrMicroRepository->get_millis() - start < 2000 && pos < sizeof(buffer) - 1) {
        if (avrMicroRepository->available()) {
            char c = avrMicroRepository->read();
            buffer[pos++] = c;
        }
    }
    buffer[pos] = '\0';
    // Cerca "+CSQ: " nella risposta
    char* csq = strstr(buffer, "+CSQ:");
    if (csq != nullptr) {
        int rssi, ber;
        if (sscanf(csq, "+CSQ: %d,%d", &rssi, &ber) == 2) {
            if (rssi >= 0 && rssi <= 31) return rssi;
        }
    }
    return -1; // Errore o fuori range
}
bool SimRepository::isGprsAttached() {
    avrMicroRepository->clearBuffer();
    avrMicroRepository->print("AT+CGATT?");
    avrMicroRepository->print(LITERAL_RETURN);

    char buffer[32];
    int pos = 0;
    unsigned long start = avrMicroRepository->get_millis();

    while (avrMicroRepository->get_millis() - start < 2000 && pos < sizeof(buffer) - 1) {
        if (avrMicroRepository->available()) {
            char c = avrMicroRepository->read();
            buffer[pos++] = c;
        }
    }
    buffer[pos] = '\0';

    char* ptr = strstr(buffer, "+CGATT:");
    if (ptr != nullptr) {
        int status;
        if (sscanf(ptr, "+CGATT: %d", &status) == 1) {
            return status == 1; // 1: attached
        }
    }

    return false; // errore o non attached
}








