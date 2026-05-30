cmake_minimum_required(VERSION 3.22.1)

project(EmuCoreXAndroidEntrypoint LANGUAGES C CXX)

add_subdirectory(
	"${CMAKE_CURRENT_LIST_DIR}/ARM_ANDROID"
	"${CMAKE_CURRENT_BINARY_DIR}/arm_android"
)
