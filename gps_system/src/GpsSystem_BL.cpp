// File: GpsSystem_BL.cpp
#include "GpsSystem_BL.h"
#if _DEBUG_FOR_SERIAL && !_ON_MOCKING_TESTS
#include <Arduino.h>
#endif
GpsSystem_BL::GpsSystem_BL(NmeaGpsActivity* gps_activity)
	: gps_activity_(gps_activity),
	sim_repository_(nullptr) {
}
GpsSystem_BL::GpsSystem_BL(SimRepository* sim_repository)
	: gps_activity_(nullptr),
	sim_repository_(sim_repository) {
}
bool GpsSystem_BL::read_and_log_coordinates(GpsConnectionStatus& out_status) {
	if (gps_activity_ == nullptr) {
		return false;
	}
	this->gps_activity_->get_repository().listen();
	const auto gps_status = gps_activity_->update_gps_status();
	out_status = gps_status;
	if (!gps_status.has_recent_coordinates) {
		return false;
	}
	/*   float latitude = static_cast<float>(gps_status.latitude_x1000000) / 1000000.0f;
	   float longitude = static_cast<float>(gps_status.longitude_x1000000) / 1000000.0f;
	   Serial.print("https://www.google.com/maps/search/?api=1&query=");
	   Serial.print(latitude, 6);
	   Serial.print(",");
	   Serial.println(longitude, 6);*/
	this->gps_activity_->get_repository().stop_listening();
	return true;
}
bool GpsSystem_BL::call_phone_number(const char* number) {
	if (sim_repository_ == nullptr) {
		return false;
	}
	sim_repository_->listen();
	return sim_repository_->call(number);
}
bool GpsSystem_BL::check_if_sms_arrived() {
	if (sim_repository_ == nullptr) {
			return false;
	}
	this->sim_repository_->listen();
	this->sim_repository_->delay(100); // Attendo un breve periodo per assicurarmi che eventuali SMS in arrivo vengano registrati.
	int sms_count = this->sim_repository_->getSmsCount();
	this->sim_repository_->delay(100); // Attendo un breve periodo per assicurarmi che eventuali SMS in arrivo vengano registrati.
	if (sms_count > 0) this->sim_repository_->deleteAllSms(); // Elimino tutti gli SMS dopo aver verificato la loro presenza.
	this->sim_repository_->stop_listening();
	return sms_count > 0;
}
void GpsSystem_BL::hang_up_phone_call() {
	if (sim_repository_ == nullptr) {
		return;
	}
	this->sim_repository_->listen();
	this->sim_repository_->hangUp();
	this->sim_repository_->stop_listening();
}
void GpsSystem_BL::startSmsReception() {
	if (sim_repository_ == nullptr) {
		return;
	}
	this->sim_repository_->listen();
	this->sim_repository_->initSmsReception();
	this->sim_repository_->stop_listening();
}
