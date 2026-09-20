# Photo Booth — Syntax & Language-Choice Soundbites

Why specific symbols, keywords, and constructs are used — not what the algorithm does, but why the *code is written the way it is*. Pair this with the behavior soundbites doc.

---

## Passing images around

- **`const cv::Mat& image` (parameter)**: "Const reference — no copy of the pixel buffer, and the function promises not to modify the caller's image."
- **`cv::Mat` returned by value (not by reference/pointer)**: "OpenCV's `cv::Mat` is reference-counted internally and cheap to move, so returning by value doesn't copy pixel data — the compiler moves it out."
- **`cv::Mat output;` declared empty, filled by `cvtColor`/`bitwise_not`**: "OpenCV allocates the right size and type for you inside the call — no need to size it manually first."
- **`.clone()` vs. plain assignment**: "Assignment (`=`) would just share the same underlying pixel buffer; `.clone()` forces an actual independent copy — needed anywhere the original still has to stay untouched."

---

## Pixel access

- **`cv::Vec3b`**: "A fixed-size 3-byte vector type — matches exactly one BGR pixel in an 8-bit image, no more, no less."
- **`image.at<cv::Vec3b>(row, column)`**: "Templated accessor — you tell it the pixel type so it knows how many bytes to read; it's bounds-checked in debug builds, which catches off-by-one row/column mistakes early."
- **`const cv::Vec3b& input_pixel` / `cv::Vec3b& output_pixel`**: "Reference, not a copy — one's read-only (`const`), one's writable, and neither copies 3 bytes unnecessarily even though 3 bytes is small; it's just consistent practice."

---

## Casts

- **`static_cast<int>(value / step)`**: "`static_cast` instead of a C-style cast — it only allows conversions the compiler considers reasonable (like double-to-int), and it's visually searchable in code, unlike `(int)value`."
- **`static_cast<unsigned char>(quantized)`**: "Explicit because we're narrowing a wider int down to a single byte — writing it explicitly signals 'yes, this truncation is intentional,' not an accident."
- **`static_cast<double>(configuration_.width)`**: "OpenCV's `set()` takes a `double` for every property regardless of what it represents — this makes the int-to-double promotion explicit rather than relying on an implicit conversion."

---

## Lookup tables and raw arrays

- **`unsigned char lookup[256]`**: "Plain C-style array, not `std::array` or `std::vector` — the size is a compile-time constant (always 256, one per byte value) and this is a hot inner loop, so the simplest, most predictable option was used."
- **`int histogram[3][256] = {}`**: "The `= {}` zero-initializes every element — without it, a plain local array would contain garbage values, and the counts would be wrong from the start."

---

## Control flow

- **`break;` after every `case`**: "Without `break`, execution would fall through into the next case — each key needs to do exactly one thing, not cascade into the next key's logic."
- **`continue;` in the rotate/quantize loops**: "Skips the rest of this loop iteration only — moves on to the next pixel/channel without exiting the whole loop."
- **`if (...) { throw ...; }` instead of returning an error code**: "These are precondition violations — a caller passing a negative level count or an empty image is a programming mistake, not a normal runtime failure, so it's treated as an exception rather than something the caller is expected to check for."

---

## `const` everywhere

- **`const` on read-only local variables (`const double step = ...`)**: "Signals — to both the compiler and a reader — that this value is computed once and never reassigned; the compiler can also apply optimizations knowing it won't change."
- **`const` member functions (`isOpen() const`, `image() const noexcept`)**: "Promises the function doesn't modify the object's state — lets you call it on a `const ImageCapture&` and lets the compiler catch you if you accidentally try to mutate something inside it."

---

## `ImageCapture`-specific language choices

- **`explicit ImageCapture(Configuration configuration)`**: "`explicit` blocks the compiler from silently converting a `Configuration` into an `ImageCapture` wherever one is expected — without it, a typo like passing a config where a camera object is expected could compile without any error."
- **Deleted copy/move constructors (`= delete`)**: "A `cv::VideoCapture` represents a live OS camera handle — copying or moving it would create ambiguity about which object actually owns the device, so both are explicitly disabled rather than left to accidentally work in some unintended way."
- **`~ImageCapture()` calling `close()`**: "RAII — Resource Acquisition Is Initialization. The camera is guaranteed to be released when the object goes out of scope, even if the caller forgets to call `close()` themselves."
- **`noexcept` on `close()`, `clearImage()`, `clearError()`**: "A destructor calls `close()`, and a destructor must never let an exception escape — `noexcept` is both a promise to the compiler and a contract that's enforced with an internal `catch (...)`."
- **`[[nodiscard]] bool isOpen() const`**: "Makes the compiler warn if you call this and throw away the answer — checking whether the camera opened and then ignoring the result is almost always a bug."
- **Trailing underscore on members (`image_`, `capture_`, `frame_number_`)**: "Naming convention that instantly tells a reader 'this is private member data,' distinguishing it from local variables or parameters with similar names."

---

## `AppConfig`-specific language choices

- **Anonymous `namespace { ... }`**: "Everything inside is only visible within this one `.cpp` file — the C++ equivalent of marking a helper function `private` to the file, so it can't clash with a same-named function elsewhere in the project."
- **`template <typename T> void readOptional(...)`**: "One function handles `int`, `bool`, `double`, and `std::string` fields alike instead of writing near-identical code four times."
- **`node.template value<T>()`**: "The `template` keyword here is required, not optional — because `T` is a template parameter, the compiler can't tell on its own that `value` is itself a template method being called, so `template` disambiguates that for it."
- **`for (const auto& [key, value] : table)`**: "Structured bindings — unpacks each table entry into named `key`/`value` variables directly in the loop header instead of accessing `.first`/`.second` on a pair."
- **`(void)value;`**: "Explicitly marks `value` as intentionally unused in that loop (only `key` is needed there) — silences a compiler warning about an unused variable without disabling the warning globally."
- **`if (const auto* camera = table["camera"].as_table())`**: "Declaring and null-checking in the same `if` — `as_table()` returns `nullptr` if that section is missing from the file, and the section is only used inside the `if` where it's guaranteed non-null."
- **`std::optional`-style check (`if (!value)`)**: "toml++'s `.value<T>()` returns an empty optional if the conversion fails — `!value` is the standard way to check an optional is empty before dereferencing it with `*value`."

---

## `ImageProcessing`-specific language choices

- **`const char* function_name` parameter in `validateImage`**: "A plain C-string literal is enough here — it's always called with a string literal like `\"quantizeImage()\"`, so there's no need for the overhead of `std::string`."
- **`std::string(function_name) + ...`**: "Converts to `std::string` right before concatenation, since you can't directly `+` two C-strings together in C++ — only `std::string`s support that operator."

---

## Miscellaneous conventions worth naming if asked

- **Ternary operator (`state.inversion_enabled ? "ON" : "OFF"`)**: "Compact inline conditional — reads as 'if enabled, use this string, otherwise that one,' avoiding a 4-line `if`/`else` just to pick a string."
- **`std::filesystem::path` for the config path**: "Type-safe path handling — correctly deals with path separators and file existence checks across platforms, instead of manipulating raw strings."
- **`CV_PI`**: "OpenCV's own high-precision π constant — avoids retyping or mistyping the digits of π, and guarantees the same precision OpenCV's own functions use internally."
- **`cv::Scalar(0, 0, 0)`**: "A 3-element value matching a BGR pixel — used here to mean 'pure black' when initializing the rotated output image."
