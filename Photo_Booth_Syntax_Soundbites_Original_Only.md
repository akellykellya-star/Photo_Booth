# Syntax & Language-Choice Soundbites — Original Code Only

Trimmed to just the syntax choices in the code this fork actually wrote or meaningfully changed. Removed everything that's identical to the course's starter template — `ImageCapture` and `AppConfig` are essentially untouched from the template (confirmed by diffing against `csalvaggio/imgs361-photo-booth`), so their language-choice patterns (RAII destructor, `explicit`, deleted copy/move constructors, anonymous namespace, `template<T>` helper, structured bindings, `[[nodiscard]]`, trailing-underscore members, etc.) aren't this fork's decisions to defend — they came with the starter code. Added new entries at the end for syntax in the new/changed functions that wasn't covered before.

---

## Pixel access (used throughout `quantizeImage`, `dynamicContrast`, `rotateImage` — all new)

- **`cv::Vec3b`**: A fixed-size 3-byte vector type — matches exactly one BGR pixel in an 8-bit image, no more, no less.
- **`image.at<cv::Vec3b>(row, column)`**: Templated accessor — you tell it the pixel type so it knows how many bytes to read; it's bounds-checked in debug builds, catching off-by-one row/column mistakes early.
- **`const cv::Vec3b& input_pixel` / `cv::Vec3b& output_pixel`**: Reference, not a copy — one's read-only (`const`), one's writable; consistent practice even though a 3-byte copy would be cheap either way.

---

## Casts and numeric literals (in `quantizeImage`, `dynamicContrast`)

- **`static_cast<int>(value / step)`**: `static_cast` instead of a C-style cast — only allows conversions the compiler considers reasonable, and it's visually searchable in code, unlike `(int)value`.
- **`static_cast<unsigned char>(quantized)`**: Explicit because it's narrowing a wider `int` down to a single byte — writing it out signals "this truncation is intentional," not an accident.
- **`256.0` instead of `256` in `step = 256.0 / levels`**: The `.0` makes this a `double` literal, forcing floating-point division. Writing `256 / levels` instead would silently perform integer division and produce badly wrong bucket sizes for most level counts (e.g. `256/3` truncates to `85`, losing the fractional remainder that floating-point division keeps).
- **`255.0` in the contrast-stretch formula** (`(value - low_value) * 255.0 / (high_value - low_value)`): Same reasoning — one `double` literal in the expression is enough to force the whole calculation into floating-point math even though every other operand is `int`.
- **Contrast with `total_pixels * low_percentile / 100`**: Here integer division is intentional and correct — a pixel count has to be a whole number, so truncating a fractional percentage of pixels is the desired behavior, unlike the two cases above.

---

## Lookup tables and raw arrays (new to `quantizeImage`/`dynamicContrast`)

- **`unsigned char lookup[256]`**: Plain C-style array, not `std::array` or `std::vector` — the size is a compile-time constant (always 256, one per byte value) in a hot inner loop, so the simplest, most predictable option was used.
- **`int histogram[3][256] = {}`**: The `= {}` zero-initializes every element — without it, a plain local array holds garbage values and the counts would be wrong from the start.

---

## Control flow (new to `quantizeImage`, `dynamicContrast`, `rotateImage`, `handleKey`)

- **`break;` after every `case` in `handleKey`**: Without it, execution falls through into the next case — each key needs to do exactly one thing, not cascade into the next key's logic.
- **`continue;` in `dynamicContrast`'s per-channel loop and `rotateImage`'s per-pixel loop**: Skips the rest of this one iteration only — moves to the next channel/pixel without exiting the whole loop.
- **`throw std::invalid_argument(...)` for bad `levels`/percentile arguments**: These are precondition violations — a caller passing a negative level count or a backwards percentile range is a programming mistake, not a normal runtime failure, so it's an exception rather than something the caller checks for via a return code.

---

## `const` on new local variables

- **`const double step = ...` in `quantizeImage`, `const int low_count = ...` in `dynamicContrast`**: Signals the value is computed once and never reassigned — helps a reader (and the compiler) trust it won't change mid-function.

---

## New additions: syntax in `processFrame`, `showPreviewFrame`, `handleKey`, `ProcessingState` not previously covered

- **Passing `int levels`, `int low_percentile`/`int high_percentile`, `double angle` by value, not by `const&`**: These are small, cheap-to-copy fundamental types — a reference would just add a layer of indirection for something that's already as cheap to copy directly. (Contrast with `const cv::Mat&`, where the pixel buffer is large enough that copying would actually cost something.)

- **`const ProcessingState& state` in `processFrame`/`showPreviewFrame` vs. `ProcessingState& state` in `handleKey`**: The `const` reference is read-only — those two functions only need to check the current toggles, never change them. `handleKey`'s reference is deliberately non-`const` because flipping a toggle or adjusting a level/angle means writing back into the caller's actual `ProcessingState` object, not a private copy.

- **In-class default member initializers in `ProcessingState`** (`bool inversion_enabled{false};`, `int quantization_levels{8};`, `double rotation_angle{0.0};`): Every field gets a safe starting value right at its declaration, so a plain `ProcessingState processing_state;` in `main()` is fully and correctly initialized with no separate constructor needed.

- **Compound assignment operators in `handleKey`** (`*=`, `/=`, `+=`, `-=`): Shorthand for "take the current value, apply the operation, write it back" — `state.quantization_levels *= 2;` is the same as `state.quantization_levels = state.quantization_levels * 2;`, just more compact and a common idiom for exactly this kind of "adjust a counter" logic.

- **Character literals as `switch`/`case` labels** (`case 'i': case 'I':`): A character literal like `'i'` is really just a small integer constant under the hood (its ASCII/character-set value) — that's *why* a `switch` on an `int` key code can match directly against `'i'` without any explicit conversion.

- **`const int key` parameter compared against char literals in `handleKey`**: Key codes come back from OpenCV's `cv::waitKeyEx` as plain `int` values, but get compared against `char` literals (`'i'`, `'q'`, etc.) — this works because of the same char-to-int relationship above; no cast is needed for the comparison to be meaningful.

- **Ternary operator in `showPreviewFrame`'s status string** (`state.inversion_enabled ? "ON" : "OFF"`): Compact inline conditional — reads as "if enabled, use this string, otherwise that one," avoiding a 4-line `if`/`else` just to pick between two literals.

- **`CV_PI` in `rotateImage`**: OpenCV's own high-precision π constant — avoids retyping or mistyping the digits of π, and guarantees the same precision OpenCV's own internal functions use.

- **`cv::Scalar(0, 0, 0)` in `rotateImage`**: A 3-element value matching a BGR pixel — used here specifically to mean "pure black" when initializing the rotated output image before any pixels are copied in.
