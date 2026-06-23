// cf.
// http://www.rubyinside.com/how-to-create-a-ruby-extension-in-c-in-under-5-minutes-100.html
// cf. http://cesare.seesaa.net/article/32850625.html
#include "Mandarin.h"
#include "ruby.h"

using namespace Formosa::Mandarin;

VALUE native_bopomofo = Qnil;

extern "C" {
void Init_native_bopomofo();
VALUE native_standard_layout_string_to_bopomofo(VALUE self, VALUE pStr);
VALUE native_bopomofo_to_absolute_order_string(VALUE self, VALUE pStr);
VALUE native_absolute_order_string_to_bopomofo(VALUE self, VALUE pStr);

typedef VALUE (*rfunc_t)(...);
}

void Init_native_bopomofo() {
  native_bopomofo = rb_define_module("NativeBopomofo");
  rb_define_singleton_method(
      native_bopomofo, "standard_layout_string_to_bopomofo",
      (rfunc_t)native_standard_layout_string_to_bopomofo, 1);
  rb_define_singleton_method(
      native_bopomofo, "bopomofo_to_absolute_order_string",
      (rfunc_t)native_bopomofo_to_absolute_order_string, 1);
  rb_define_singleton_method(
      native_bopomofo, "absolute_order_string_to_bopomofo",
      (rfunc_t)native_absolute_order_string_to_bopomofo, 1);
}

VALUE native_standard_layout_string_to_bopomofo(VALUE self, VALUE pStr) {
  VALUE rStr = StringValue(pStr);
  size_t rStrLen = RSTRING(rStr)->len;
  char* rStrPtr = RSTRING(rStr)->ptr;
  if (!rStrPtr) return Qnil;

  char* input = (char*)calloc(1, rStrLen + 1);
  memcpy(input, rStrPtr, rStrLen);

  const BopomofoKeyboardLayout* layout =
      BopomofoKeyboardLayout::StandardLayout();
  BPMF syllable = layout->syllableFromKeySequence(input);
  string output = syllable.composedString();

  free(input);
  return rb_str_new2(output.c_str());
}

VALUE native_bopomofo_to_absolute_order_string(VALUE self, VALUE pStr) {
  VALUE rStr = StringValue(pStr);
  size_t rStrLen = RSTRING(rStr)->len;
  char* rStrPtr = RSTRING(rStr)->ptr;
  if (!rStrPtr) return Qnil;

  char* input = (char*)calloc(1, rStrLen + 1);
  memcpy(input, rStrPtr, rStrLen);

  BPMF syllable = BPMF::FromComposedString(input);
  string output = syllable.absoluteOrderString();

  free(input);
  return rb_str_new2(output.c_str());
}

VALUE native_absolute_order_string_to_bopomofo(VALUE self, VALUE pStr) {
  VALUE rStr = StringValue(pStr);
  size_t rStrLen = RSTRING(rStr)->len;
  char* rStrPtr = RSTRING(rStr)->ptr;
  if (!rStrPtr) return Qnil;

  char* input = (char*)calloc(1, rStrLen + 1);
  memcpy(input, rStrPtr, rStrLen);

  BPMF syllable = BPMF::FromAbsoluteOrderString(input);
  string output = syllable.composedString();

  free(input);
  return rb_str_new2(output.c_str());
}
