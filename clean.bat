make -f emu.mk clean
idf.py clean fullclean
rmdir /s /q build
rmdir /s /q spiffs_image
