#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
 
#include <opencv2/imgcodecs.hpp>
 
#include "photo_booth/AppConfig.hpp"
#include "photo_booth/ImageCapture.hpp"
 
int main(int argc, char* argv[]) {
  try {
    //
    // Determine which configuration file to use.
    //
    std::filesystem::path config_path{"config.toml"};
 
    if (argc > 2) {
      std::cerr
          << "Usage: "
          << argv[0]
          << " [config.toml]\n";
 
      return EXIT_FAILURE;
    }
 
    if (argc == 2) {
      const std::string argument{argv[1]};
 
      if (argument == "-h" || argument == "--help") {
        std::cout
            << "Usage: "
            << argv[0]
            << " [config.toml]\n\n"
            << "Captures one image from a built-in or USB camera.\n";
 
        return EXIT_SUCCESS;
      }
 
      config_path = argument;
    }
 
    //
    // Load the application configuration.
    //
    const auto config =
        photo_booth::loadConfig(config_path);
 
    //
    // Configure and open the camera.
    //
    auto capture_config =
        photo_booth::makeImageCaptureConfiguration(
            config.camera);
    photo_booth::ImageCapture camera{capture_config};
 
    bool camera_opened = false;
    const int configured_device = capture_config.device_index;
 
    for (int device = configured_device;
         device < configured_device + 3;
         ++device) {
      capture_config.device_index = device;
      if (camera.open(capture_config)) {
        camera_opened = true;
        break;
      }
    }
 
    if (!camera_opened) {
      std::cerr
          << camera.errorMessage()
          << '\n';
 
      return EXIT_FAILURE;
    }
 
    //
    // Discard startup frames while auto-exposure and white balance settle.
    //
    for (int frame = 0;
         frame < config.capture.warmup_frames;
         ++frame) {
 
      if (!camera.read()) {
        std::cerr
            << camera.errorMessage()
            << '\n';
 
        return EXIT_FAILURE;
      }
    }
 
    //
    // Always capture one final frame, even when warmup_frames is zero.
    //
    if (!camera.read()) {
      std::cerr
          << camera.errorMessage()
          << '\n';
 
      return EXIT_FAILURE;
    }
 
    //
    // Save the acquired image.
    //
    if (!cv::imwrite(
            config.capture.output_filename,
            camera.image())) {
 
      std::cerr
          << "Could not write "
          << config.capture.output_filename
          << ".\n";
 
      return EXIT_FAILURE;
    }
 
    std::cout
        << "Saved "
        << config.capture.output_filename
        << " using camera "
        << config.camera.device
        << ".\n";
 
  } catch (const cv::Exception& error) {
    std::cerr
        << "OpenCV error: "
        << error.what()
        << '\n';
 
    return EXIT_FAILURE;
 
  } catch (const std::exception& error) {
    std::cerr
        << "Error: "
        << error.what()
        << '\n';
 
    return EXIT_FAILURE;
  }
 
  return EXIT_SUCCESS;
}
 
