#include <truffleruby-impl.h>
#include <ruby/encoding.h>
#include <truffleruby/internal/symbol.h>

// Symbol and ID, rb_sym*, rb_id*

ID rb_to_id(VALUE name) {
  return SYM2ID(RUBY_INVOKE(name, "to_sym"));
}

#undef rb_intern
ID rb_intern(const char *string) {
  return SYM2ID(RUBY_CEXT_INVOKE("rb_intern", rb_str_new_cstr(string)));
}

ID rb_intern2(const char *string, long length) {
  return SYM2ID(RUBY_CEXT_INVOKE("rb_intern", rb_str_new(string, length)));
}

ID rb_intern3(const char *name, long len, rb_encoding *enc) {
  return SYM2ID(RUBY_CEXT_INVOKE("rb_intern3", rb_str_new(name, len), rb_enc_from_encoding(enc)));
}

VALUE rb_sym2str(VALUE string) {
  return RUBY_INVOKE(string, "to_s");
}

const char *rb_id2name(ID id) {
    return RSTRING_PTR(rb_id2str(id));
}

VALUE rb_id2str(ID id) {
  return RUBY_CEXT_INVOKE("rb_id2str", ID2SYM(id));
}

int rb_is_class_id(ID id) {
  return polyglot_as_boolean(RUBY_CEXT_INVOKE_NO_WRAP("rb_is_class_id", ID2SYM(id)));
}

int rb_is_const_id(ID id) {
  return polyglot_as_boolean(RUBY_CEXT_INVOKE_NO_WRAP("rb_is_const_id", ID2SYM(id)));
}

int rb_is_instance_id(ID id) {
  return polyglot_as_boolean(RUBY_CEXT_INVOKE_NO_WRAP("rb_is_instance_id", ID2SYM(id)));
}

VALUE rb_check_symbol_cstr(const char *ptr, long len, rb_encoding *enc) {
  VALUE str = rb_enc_str_new(ptr, len, enc);
  return RUBY_CEXT_INVOKE("rb_check_symbol_cstr", str);
}

VALUE rb_sym_to_s(VALUE sym) {
  return RUBY_INVOKE(sym, "to_s");
}

#undef rb_sym2id
ID rb_sym2id(VALUE sym) {
  return rb_tr_sym2id(sym);;
}

#undef rb_id2sym
VALUE rb_id2sym(ID x) {
  return rb_tr_wrap(rb_tr_id2sym(x));
}

VALUE rb_to_symbol(VALUE name) {
  return RUBY_CEXT_INVOKE("rb_to_symbol", name);
}

// Usually found in parse.c
static inline int is_identchar(const char *ptr, const char *MAYBE_UNUSED(ptr_end), rb_encoding *enc) {
  return rb_enc_isalnum((unsigned char)*ptr, enc) || *ptr == '_' || !ISASCII(*ptr);
}

static int is_special_global_name(const char *m, const char *e, rb_encoding *enc) {
  int mb = 0;

  if (m >= e) return 0;
  if (is_global_name_punct(*m)) {
    ++m;
  }
  else if (*m == '-') {
    if (++m >= e) return 0;
    if (is_identchar(m, e, enc)) {
      if (!ISASCII(*m)) mb = 1;
      m += rb_enc_mbclen(m, e, enc);
    }
  }
  else {
    if (!ISDIGIT(*m)) return 0;
    do {
      if (!ISASCII(*m)) mb = 1;
      ++m;
    } while (m < e && ISDIGIT(*m));
  }
  return m == e ? mb + 1 : 0;
}

static int rb_sym_constant_char_p(const char *name, long nlen, rb_encoding *enc) {
  int c, len;
  const char *end = name + nlen;

  if (nlen < 1) return FALSE;
  if (ISASCII(*name)) return ISUPPER(*name);
  c = rb_enc_precise_mbclen(name, end, enc);
  if (!MBCLEN_CHARFOUND_P(c)) return FALSE;
  len = MBCLEN_CHARFOUND_LEN(c);
  c = rb_enc_mbc_to_codepoint(name, end, enc);
  if (ONIGENC_IS_UNICODE(enc)) {
    static int ctype_titlecase = 0;
    if (rb_enc_isupper(c, enc)) return TRUE;
    if (rb_enc_islower(c, enc)) return FALSE;
    if (!ctype_titlecase) {
      static const UChar cname[] = "titlecaseletter";
      static const UChar *const end = cname + sizeof(cname) - 1;
#ifdef TRUFFLERUBY
      rb_tr_error("ONIGENC_PROPERTY_NAME_TO_CTYPE not yet implemented");
#else
      ctype_titlecase = ONIGENC_PROPERTY_NAME_TO_CTYPE(enc, cname, end);
#endif
    }
    if (rb_enc_isctype(c, ctype_titlecase, enc)) return TRUE;
  }
  else {
    /* fallback to case-folding */
    OnigUChar fold[ONIGENC_GET_CASE_FOLD_CODES_MAX_NUM];
    const OnigUChar *beg = (const OnigUChar *)name;
#ifdef TRUFFLERUBY
    int r = ONIGENC_MBC_CASE_FOLD(enc, ONIGENC_CASE_FOLD, &beg, (const OnigUChar *)end, fold);
#else
    int r = enc->mbc_case_fold(ONIGENC_CASE_FOLD,
                   &beg, (const OnigUChar *)end,
                   fold, enc);
#endif
    if (r > 0 && (r != len || memcmp(fold, name, r)))
      return TRUE;
  }
  return FALSE;
}

int rb_enc_symname_type(const char *name, long len, rb_encoding *enc, unsigned int allowed_attrset) {
  const char *m = name;
  const char *e = m + len;
  int type = ID_JUNK;

  if (!rb_enc_asciicompat(enc)) return -1;
  if (!m || len <= 0) return -1;
  switch (*m) {
    case '\0':
      return -1;

    case '$':
      type = ID_GLOBAL;
      if (is_special_global_name(++m, e, enc)) return type;
      goto id;

    case '@':
      type = ID_INSTANCE;
      if (*++m == '@') {
        ++m;
        type = ID_CLASS;
      }
      goto id;

    case '<':
      switch (*++m) {
        case '<': ++m; break;
        case '=': if (*++m == '>') ++m; break;
        default: break;
      }
      break;

    case '>':
      switch (*++m) {
        case '>': case '=': ++m; break;
      }
      break;

    case '=':
      switch (*++m) {
        case '~': ++m; break;
        case '=': if (*++m == '=') ++m; break;
        default: return -1;
      }
      break;

    case '*':
      if (*++m == '*') ++m;
      break;

    case '+': case '-':
      if (*++m == '@') ++m;
      break;

    case '|': case '^': case '&': case '/': case '%': case '~': case '`':
      ++m;
      break;

    case '[':
      if (m[1] != ']') goto id;
      ++m;
      if (*++m == '=') ++m;
      break;

    case '!':
      if (len == 1) return ID_JUNK;
      switch (*++m) {
        case '=': case '~': ++m; break;
        default:
          if (allowed_attrset & (1U << ID_JUNK)) goto id;
          return -1;
      }
      break;

    default:
      type = rb_sym_constant_char_p(m, e-m, enc) ? ID_CONST : ID_LOCAL;
    id:
      if (m >= e || (*m != '_' && !ISALPHA(*m) && ISASCII(*m))) {
        if (len > 1 && *(e-1) == '=') {
          type = rb_enc_symname_type(name, len-1, enc, allowed_attrset);
          if (type != ID_ATTRSET) return ID_ATTRSET;
        }
        return -1;
      }
      while (m < e && is_identchar(m, e, enc)) m += rb_enc_mbclen(m, e, enc);
      if (m >= e) break;
      switch (*m) {
        case '!': case '?':
          if (type == ID_GLOBAL || type == ID_CLASS || type == ID_INSTANCE) return -1;
          type = ID_JUNK;
          ++m;
          if (m + 1 < e || *m != '=') break;
          /* fall through */
        case '=':
          if (!(allowed_attrset & (1U << type))) return -1;
          type = ID_ATTRSET;
          ++m;
          break;
      }
      break;
  }
  return m == e ? type : -1;
}

static int rb_str_symname_type(VALUE name, unsigned int allowed_attrset) {
  const char *ptr = StringValuePtr(name);
  long len = RSTRING_LEN(name);
  int type = rb_enc_symname_type(ptr, len, rb_enc_get(name), allowed_attrset);
  RB_GC_GUARD(name);
  return type;
}

int id_type(ID id) {
  const char* cstr = rb_id2name(id);
  VALUE name = rb_str_new_cstr(cstr);
  return rb_str_symname_type(name, 0);
}