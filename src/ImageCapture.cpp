#include "photo_booth/ImageCapture.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace photo_booth {

namespace {

std::string configurationError(
    const ImageCapture::Configuration& configuration) {
  if (configuration.device_index < 0) {
    return "Camera device index must be >= 0.";
  }

  if (configuration.width < 0) {
    return "Camera width must be >= 0.";
  }

  if (configuration.height < 0) {
    return "Camera height must be >= 0.";
  }

  if (!std::isfinite(configuration.frames_per_second) ||
      configuration.frames_per_second < 0.0) {
    return "Camera frame rate must be finite and >= 0.";
  }

  if (!configuration.fourcc.empty() && configuration.fourcc.size() != 4) {
    return "FOURCC must be empty or contain exactly four characters.";
  }

  return {};
}

}  // namespace

ImageCapture::ImageCapture(Configuration configuration)
    : configuration_{std::move(configuration)} {
  const std::string error = configurationError(configuration_);
  if (!error.empty()) {
    throw std::invalid_argument("Invalid ImageCapture configuration: " + error);
  }
}

ImageCapture::~ImageCapture() {
  close();
}

bool ImageCapture::open() {
  const std::string validation_error = configurationError(configuration_);
  if (!validation_error.empty()) {
    setError(validation_error);
    return false;
  }

  close();
  clearError();

  try {
    if (!capture_.open(configuration_.device_index,
                       configuration_.api_preference)) {
      setError("Unable to open camera device " +
               std::to_string(configuration_.device_index) + ".");
      return false;
    }

    applyConfiguration();

    if (!capture_.isOpened()) {
      setError("The camera closed while its configuration was applied.");
      close();
      return false;
    }
  } catch (const cv::Exception& exception) {
    setError(std::string{"OpenCV could not open the camera: "} +
             exception.what());
    close();
    return false;
  }

  frame_number_ = 0;
  clearImage();
  clearError();
  return true;
}

bool ImageCapture::open(const Configuration& configuration) {
  const std::string validation_error = configurationError(configuration);
  if (!validation_error.empty()) {
    setError(validation_error);
    return false;
  }

  configuration_ = configuration;
  return open();
}

void ImageCapture::close() noexcept {
  try {
    if (capture_.isOpened()) {
      capture_.release();
    }
  } catch (...) {
  }

  clearImage();
  frame_number_ = 0;
}

bool ImageCapture::isOpen() const {
  try {
    return capture_.isOpened();
  } catch (const cv::Exception&) {
    return false;
  }
}

bool ImageCapture::read() {
  clearError();
//Local next_image variable: Reads into a temporary first so a failed read can never corrupt the last good frame.
//CV_8UC3 check here too: If the camera hands back anything else, it's rejected right here.
//std::move into image_: Moves instead of copies, avoids duplicating the pixel buffer.
  if (!isOpen()) {
    setError("Cannot capture an image because the camera is not open.");
    clearImage();
    return false;
  }

  cv::Mat next_image;

  try {
    if (!capture_.read(next_image) || next_image.empty()) {
      setError("The camera did not return a valid image.");
      clearImage();
      return false;
    }
  } catch (const cv::Exception& exception) {
    setError(std::string{"OpenCV failed while capturing an image: "} +
             exception.what());
    clearImage();
    return false;
  }

  if (next_image.type() != CV_8UC3) {
    setError(
        "The camera returned an image that is not an 8-bit, three-channel "
        "BGR image (CV_8UC3).");
    clearImage();
    return false;
  }

  image_ = std::move(next_image);
  ++frame_number_;
  return true;
}

const cv::Mat& ImageCapture::image() const noexcept {
  return image_;
}

cv::Mat ImageCapture::imageCopy() const {
  return image_.clone();
}

bool ImageCapture::hasImage() const {
  return !image_.empty();
}

const ImageCapture::Configuration& ImageCapture::configuration()
    const noexcept {
  return configuration_;
}

ImageCapture::CameraInfo ImageCapture::cameraInfo() const {
  CameraInfo information;
  information.device_index = configuration_.device_index;

  if (!isOpen()) {
    return information;
  }

  information.backend_id =
      static_cast<int>(std::lround(property(cv::CAP_PROP_BACKEND)));

  information.width =
      static_cast<int>(std::lround(property(cv::CAP_PROP_FRAME_WIDTH)));

  information.height =
      static_cast<int>(std::lround(property(cv::CAP_PROP_FRAME_HEIGHT)));

  information.frames_per_second = property(cv::CAP_PROP_FPS);

  information.fourcc = fourccToString(
      static_cast<int>(std::lround(property(cv::CAP_PROP_FOURCC))));

  try {
    information.backend_name = capture_.getBackendName();
  } catch (const cv::Exception&) {
    information.backend_name.clear();
  }

  return information;
}

bool ImageCapture::setProperty(int property_id, double value) {
  clearError();

  if (!isOpen()) {
    setError("Cannot set a camera property because the camera is not open.");
    return false;
  }

  try {
    if (!capture_.set(property_id, value)) {
      setError("The camera or capture backend rejected property " +
               std::to_string(property_id) + ".");
      return false;
    }
  } catch (const cv::Exception& exception) {
    setError(std::string{"OpenCV failed while setting a camera property: "} +
             exception.what());
    return false;
  }

  return true;
}

double ImageCapture::property(int property_id) const {
  if (!isOpen()) {
    return 0.0;
  }

  try {
    return capture_.get(property_id);
  } catch (...) {
    return 0.0;
  }
}

const std::string& ImageCapture::errorMessage() const noexcept {
  return error_message_;
}

std::uint64_t ImageCapture::frameNumber() const noexcept {
  return frame_number_;
}

void ImageCapture::applyConfiguration() {
  // FOURCC is applied before dimensions and frame rate because some
  // camera backends expose different modes for different pixel formats.
  //Conditional property sets (if width > 0, etc.): Zero means leave the camera's default alone]
  //CAP_PROP_CONVERT_RGB forced on: This is what actually guarantees BGR output.
  if (!configuration_.fourcc.empty()) {
    capture_.set(cv::CAP_PROP_FOURCC,
                 static_cast<double>(fourccFromString(configuration_.fourcc)));
  }

  if (configuration_.width > 0) {
    capture_.set(cv::CAP_PROP_FRAME_WIDTH,
                 static_cast<double>(configuration_.width));
  }

  if (configuration_.height > 0) {
    capture_.set(cv::CAP_PROP_FRAME_HEIGHT,
                 static_cast<double>(configuration_.height));
  }

  if (configuration_.frames_per_second > 0.0) {
    capture_.set(cv::CAP_PROP_FPS, configuration_.frames_per_second);
  }

  // OpenCV names this property CAP_PROP_CONVERT_RGB, but decoded color
  // images are returned using OpenCV's conventional BGR channel order.
  capture_.set(cv::CAP_PROP_CONVERT_RGB, 1.0);
}

void ImageCapture::clearImage() noexcept {
  image_.release();
}

void ImageCapture::setError(std::string message) {
  error_message_ = std::move(message);
}

void ImageCapture::clearError() noexcept {
  error_message_.clear();
}

int ImageCapture::fourccFromString(const std::string& value) {
  if (value.size() != 4) {
    throw std::invalid_argument("FOURCC must contain exactly four characters.");
  }

  return cv::VideoWriter::fourcc(value[0], value[1], value[2], value[3]);
}

std::string ImageCapture::fourccToString(int value) {
  if (value == 0) {
    return {};
  }

  std::string result(4, '\0');

  result[0] = static_cast<char>(value & 0xFF);
  result[1] = static_cast<char>((value >> 8) & 0xFF);
  result[2] = static_cast<char>((value >> 16) & 0xFF);
  result[3] = static_cast<char>((value >> 24) & 0xFF);

  for (char character : result) {
    const auto code = static_cast<unsigned char>(character);
    if (code < 32 || code > 126) {
      return {};
    }
  }

  return result;
}

std::ostream& operator<<(std::ostream& output,
                         const ImageCapture::CameraInfo& information) {
  const auto original_flags = output.flags();
  const auto original_precision = output.precision();

  output << "Camera device: " << information.device_index << '\n'
         << "Backend: ";

  if (information.backend_name.empty()) {
    output << "unknown";
  } else {
    output << information.backend_name;
  }

  output << " (" << information.backend_id << ")\n"
         << "Image size: " << information.width << " x " << information.height
         << '\n'
         << "Frame rate: " << std::fixed << std::setprecision(2)
         << information.frames_per_second << " fps\n"
         << "FOURCC: ";

  if (information.fourcc.empty()) {
    output << "unknown";
  } else {
    output << information.fourcc;
  }

  output.flags(original_flags);
  output.precision(original_precision);
  return output;
}

std::ostream& operator<<(std::ostream& output, const ImageCapture& camera) {
  return output << camera.cameraInfo();
}

}  // namespace photo_booth
