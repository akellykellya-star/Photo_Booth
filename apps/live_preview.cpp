#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
 
#include <opencv2/highgui.hpp>
 
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
            << "Displays a live preview from a built-in or USB camera.\n";
 
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
    photo_booth::ImageCapture camera(capture_config);
 
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
 
    std::cout << camera << '\n';
 
    cv::namedWindow(
        config.preview.window_name,
        cv::WINDOW_AUTOSIZE);
 
    while (true) {
      if (!camera.read()) {
        std::cerr
            << camera.errorMessage()
            << '\n';
 
        return EXIT_FAILURE;
      }
 
      cv::Mat preview_frame = camera.image().clone();
 
      switch (config.preview.rotation) {
        case 0:
          break;
 
        case 90:
          cv::rotate(
              preview_frame,
              preview_frame,
              cv::ROTATE_90_CLOCKWISE);
          break;
 
        case 180:
          cv::rotate(
              preview_frame,
              preview_frame,
              cv::ROTATE_180);
          break;
 
        case 270:
          cv::rotate(
              preview_frame,
              preview_frame,
              cv::ROTATE_90_COUNTERCLOCKWISE);
          break;
      }
 
      if (config.preview.mirror) {
        cv::flip(
            preview_frame,
            preview_frame,
            1);
      }
 
      cv::imshow(
          config.preview.window_name,
          preview_frame);
 
      const int key = cv::waitKey(1);
 
      if (key == 27 || key == 'q' || key == 'Q') {
        break;
      }
    }
 
    cv::destroyAllWindows();
 
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
 
