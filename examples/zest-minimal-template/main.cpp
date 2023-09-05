#include <zest.h>

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
}

int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_Initialise(&create_info);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}