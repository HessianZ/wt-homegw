#file(GLOB_RECURSE MENJIN_SOURCES ./*.c)

idf_component_register(
        SRC_DIRS
        "."
        "app"
        "system"
        "http"

        INCLUDE_DIRS
        "."
        "app"
        "system"
        "http"

        EMBED_TXTFILES server_root_cert.pem
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
