typedef struct GapBuffer GapBuffer;

GapBuffer *gbf_new(char *file_name, size_t file_size);
void gbf_free(GapBuffer *gbuf);
char *gbf_text(GapBuffer *gbuf);
void gbf_insert(GapBuffer *gbuf, char *s, size_t offset);
void gbf_delete(GapBuffer *gbuf, size_t offset, size_t bytes);
void gbf_clear(GapBuffer *gbuf);
char gbf_at(GapBuffer *gbuf, size_t offset);

