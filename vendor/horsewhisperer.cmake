include(ExternalProject)

# Add an external project to build horsewhisperer
externalproject_add(
    horsewhisperer
    PREFIX "${PROJECT_BINARY_DIR}"
    URL "file://${VENDOR_DIRECTORY}/horsewhisperer-0.11.1.zip"
    URL_MD5 "160bb360af58f69c566e1278f6fd8207"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)
externalproject_get_property(horsewhisperer SOURCE_DIR)
set(HORSEWHISPERER_INCLUDE_DIRS "${SOURCE_DIR}/include")
