
  // Call in the serialized network context after headers, before any body byte.
  bool enableStrictMultipart205(size_t maxBody);
  bool strictMultipartComplete205() const;
  bool strictMultipartError205() const;
