set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(SOURCE_DIR "${ROOT_DIR}/src")
set(SOURCE_FILES
	"${SOURCE_DIR}/main.cpp"
)

source_group(
	TREE "${ROOT_DIR}"
	FILES
		${SOURCE_FILES}
)

add_executable(
	"${PROJECT_NAME}"
	${SOURCE_FILES}
	"${VERSION_HEADER}"
	"${ROOT_DIR}/.clang-format"
	"${ROOT_DIR}/.editorconfig"
)

target_compile_features(
	"${PROJECT_NAME}"
	PUBLIC
		cxx_std_20
)

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		"${SOURCE_DIR}"
)

find_package(binary_io REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)
find_package(mmio REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		binary_io::binary_io
		fmt::fmt
		mmio::mmio
		spdlog::spdlog
)
