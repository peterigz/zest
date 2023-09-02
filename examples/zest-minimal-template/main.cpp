#include <zest.h>

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
}

int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();

	zest_Initialise(&create_info);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}