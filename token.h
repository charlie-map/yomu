#ifndef __TOKEN_L__
#define __TOKEN_L__

typedef Yomu {
	char *tag;

	int data_index, *max_data;
	char *data; // for a singular type (like a char *)

	int *max_attr_tag, attr_tag_index;
	char **attribute; // holds any attributes for this tag
	// the attribute name is always an even position and
	// the attribute itself is odd

	int children_data_pos; // where the children occur within the
		// overall scheme of this token
		// ** mainly for token_read_all_data() to ensure the data
		// is read from the correct position
	int *max_children, children_index;
	struct Yomu **children; // for nest children tags

	struct Yomu *parent;
} yomu_t;

typedef struct YomuFunctions yomu;

yomu_t *yomu(char reader_type, char *filename);

int token_has_classname(token_t *token, char *classname);

char *token_attr(token_t *token, char *attrname);

token_t **token_children(token_t *parent);
token_t *grab_token_parent(token_t *curr_token);

token_t *grab_token_by_tag(token_t *start_token, char *tag_name);
token_t *grab_token_by_tag_maxsearch(token_t *start_token, char *tag_name, int max_search);
token_t *grab_token_by_tag_matchparam(token_t *start_token, char *tag_name, int (*match)(token_t *));
token_t *grab_token_by_tag_matchparam_maxsearch(token_t *start_token, char *tag_name, int (*match)(token_t *), int max_search);
token_t **grab_tokens_by_tag(token_t *start_token, char *tags_name, int *spec_token_max);

token_t *grab_token_by_classname(token_t *search_token, char *classname);

char **token_get_tag_data(token_t *search_token, char *tag_name, int *max_tag);
char *token_read_all_data(token_t *search_token, int *data_max, void *block_tag, void *(*is_blocked)(void *, char *));

char *data_at_token(token_t *curr_token);
int update_token_data(token_t *curr_token, char *new_data, int *new_data_len);

int destroy_token(token_t *curr_token);

#endif
