#ifndef INCLUDED_UI_LANGUAGE_H
#define INCLUDED_UI_LANGUAGE_H

/* The interface uses the same supported locales as tutorials and help, with
 * its own preference. Unsupported codes select English. */
const char *ui_language(void);
void ui_set_language(const char *code);
void ui_language_cleanup(void);

/* Use the English label or format string as the key. Translate the template
 * before formatting so names, bindings, and other arguments stay intact.
 * Translated strings remain valid until ui_language_cleanup(). */
const char *ui_text(const char *english);

/* Only gameplay message templates participate in automatic log translation. */
const char *ui_message_text(const char *english);

#endif
