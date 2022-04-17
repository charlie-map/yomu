#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "yomu.h"

int is_range(char c);
int delimeter_check(char curr_char, char *delims);
char **split_string(char *full_string, char delimeter, int *arr_len, char *extra, ...);

void *resize_array(void *array, int *max_size, int current_index, size_t singleton_size) {
	while (current_index >= *max_size) {
		*max_size *= 2;

		array = realloc(array, singleton_size * *max_size);
	}

	return array;
}

typedef struct Match {
	char *match_tag; // used as input to the following function
	int (*match)(yomu_t *, char *);
} match_t;
match_t *create_matches(char *match_param, int *match_param_length);

struct Yomu {
	int deep_copy;
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
};

yomu_t *create_token(char *tag, yomu_t *parent, int deep_copy) {
	yomu_t *new_token = malloc(sizeof(yomu_t));

	new_token->deep_copy = deep_copy;
	new_token->tag = tag;

	new_token->data = malloc(sizeof(char) * 8);
	memset(new_token->data, '\0', sizeof(char) * 8);
	new_token->max_data = malloc(sizeof(int));
	*new_token->max_data = 8;
	new_token->data_index = 0;

	new_token->max_attr_tag = malloc(sizeof(int));
	*new_token->max_attr_tag = 8;
	new_token->attr_tag_index = 0;
	new_token->attribute = malloc(sizeof(char *) * *new_token->max_attr_tag);

	new_token->max_children = malloc(sizeof(int));
	*new_token->max_children = 8;
	new_token->children_index = 0;
	new_token->children = malloc(sizeof(yomu_t *) * *new_token->max_children);

	new_token->parent = parent;

	return new_token;
}

/*
	HASHMAP INITIALIZATION
*/
typedef struct ReturnHashmap { // used only for type 1
	void **payload;
	int payload__length;
} hashmap__response;

/*
	vtableKeyStore is used for using unknown type keys in the 
	hashmap. Because the variable type is unknown, the use
	of this struct plus the function (some function name)__hashmap() can
	be used to decipher any type of possible key in the hashmap

	SEE bottom of the page for the function declarations and uses
*/
typedef struct VTable {
	void *key;

	// these define how we utilize the key
	// DEFAULT behavior is using a char * setup
	void (*printKey)(void *);
	int (*compareKey)(void *, void *);
	void (*destroyKey)(void *);
} vtableKeyStore;

typedef struct ll_def {
	struct ll_def *next;
	
	vtableKeyStore key;
	int max__arrLength, arrIndex; // only for hash type 1
	int isArray;
	void *ll_meat; // single value pointer
				   // for hash__type = 0
				   // or array for
				   // hash__type = 1
} ll_main_t;

struct Store {
	int hashmap__size;
	// takes a type of hashmap
	// 0: replace value
	// 1: append to growing value list
	int hash__type;
	ll_main_t **map;

	// used for printing the hashmap values
	void (*printer)(void *);
	// destroying hashmap values
	void (*destroy)(void *);
};

hashmap *make__hashmap(int hash__type, void (*printer)(void *), void (*destroy)(void *));

void **keys__hashmap(hashmap *hash__m, int *max_key);
void *get__hashmap(hashmap *hash__m, void *key);

int print__hashmap(hashmap *hash__m);

int delete__hashmap(hashmap *hash__m, void *key);

int deepdestroy__hashmap(hashmap *hash);

int insert__hashmap(hashmap *hash__m, void *key, void *value, ...);

// simple key type functions
void printCharKey(void *characters);
int compareCharKey(void *characters, void *otherValue);
void destroyCharKey(void *characters);

void printIntKey(void *integer);
int compareIntKey(void *integer, void *otherValue);
void destroyIntKey(void *integer);

const int MAX_BUCKET_SIZE = 5;
const int START_SIZE = 1023; // how many initial buckets in array



int add_token_rolling_data(yomu_t *token, char data) {
	token->data[token->data_index] = data;

	token->data_index++;

	token->data = resize_array(token->data, token->max_data, token->data_index, sizeof(char));
	token->data[token->data_index] = '\0';

	return 0;
}

int update_token_data(yomu_t *curr_token, char *new_data, int *new_data_len) {
	// realloc data to fit new_data_len:
	*curr_token->max_data = *new_data_len;
	curr_token->data = realloc(curr_token->data, sizeof(char) * *new_data_len);

	strcpy(curr_token->data, new_data);

	return 0;
}

int add_token_attribute(yomu_t *token, char *tag, char *attribute) {
	token->attribute[token->attr_tag_index++] = tag;
	token->attribute[token->attr_tag_index++] = attribute;

	// check for resize
	token->attribute = resize_array(token->attribute, token->max_attr_tag, token->attr_tag_index, sizeof(char *));

	return 0;
}

char *token_attr(yomu_t *token, char *attr) {
	// search for attr
	int read_attr;
	for (read_attr = 0; read_attr < token->attr_tag_index; read_attr++) {
		if (strcmp(token->attribute[read_attr], attr) == 0)
			return token->attribute[read_attr + 1];
	}

	return "";
}

int set_attr(yomu_t *y, char *attr, char *new_val) {
	// search for attr
	int read_attr;
	for (read_attr = 0; read_attr < y->attr_tag_index; read_attr++) {
		if (strcmp(y->attribute[read_attr], attr) == 0)
			break;
	}

	char *new_new_val = malloc(sizeof(char) * strlen(new_val));
	strcpy(new_new_val, new_val);

	// need to add new attribute entirely
	if (read_attr == y->attr_tag_index) {
		char *new_attr = malloc(sizeof(char) * strlen(attr));
		strcpy(new_attr, attr);

		add_token_attribute(y, new_attr, new_new_val);

		return 0;
	}

	// otherwise just change value:
	free(y->attribute[read_attr + 1]);
	y->attribute[read_attr + 1] = new_new_val;

	return 0;
}

int add_token_children(yomu_t *token_parent, yomu_t *child) {
	token_parent->children[token_parent->children_index] = child;

	token_parent->children_index++;

	// check for resize
	token_parent->children = resize_array(token_parent->children, token_parent->max_children, token_parent->children_index, sizeof(yomu_t *));

	return 0;
}

yomu_t **token_children(yomu_t *parent) {
	return parent->children;
}

yomu_t *grab_token_children(yomu_t *parent) {
	if (parent->children_index == 0)
		return NULL;

	return parent->children[parent->children_index - 1];
}

yomu_t *grab_token_parent(yomu_t *curr_token) {
	return curr_token->parent;
}

char *data_at_token(yomu_t *curr_token) {
	return curr_token->data;
}

int grab_tokens_by_match_helper(yomu_t ***curr_found_tokens, yomu_t *start_token, int (*is_match)(yomu_t *, char *), char *search, int *found_token_length, int index_found_token, int recur) {
	for (int check_children = 0; check_children < start_token->children_index; check_children++) {
		// compare:
		if (is_match(start_token->children[check_children], search)) {
			(*curr_found_tokens)[index_found_token] = start_token->children[check_children];
			index_found_token++;

			*curr_found_tokens = resize_array(*curr_found_tokens, found_token_length, index_found_token, sizeof(yomu_t *));
		}

		// search children
		if (recur)
			index_found_token = grab_tokens_by_match_helper(curr_found_tokens, start_token->children[check_children], is_match, search, found_token_length, index_found_token, recur);
	}

	return index_found_token;
}

yomu_t **grab_tokens_by_match(yomu_t *start_token, int (*match)(yomu_t *, char *), char *search, int *length, int depth) {
	*length = 8;
	yomu_t **tokens = malloc(sizeof(yomu_t *) * *length);

	int token_number = grab_tokens_by_match_helper(&tokens, start_token, match, search, length, 0, depth);
	*length = token_number;
	return tokens;
}

// DFS for first occurrence of a match
yomu_t *grab_token_by_match(yomu_t *y, int (*match)(yomu_t *, char *), char *search, int direction) {
	// search through current y to find a search term
	// start pos depends on direction:
	// 0 for starting at 0 and going to children_index
	// 1 for starting at childrend_index and going to 0
	for (int find_match = direction ? y->children_index - 1 : 0; direction ? find_match >= 0 : find_match < y->children_index; find_match += direction ? -1 : 1) {
		if (match(y->children[find_match], search))
			return y->children[find_match];

		// check children
		yomu_t *test_children = grab_token_by_match(y->children[find_match], match, search, direction);
		if (test_children)
			return test_children;
	}

	return NULL;
}

int destroy_token(yomu_t *curr_token) {
	free(curr_token->tag);
	free(curr_token->data);

	// free attributes
	for (int free_attribute = 0; free_attribute < curr_token->attr_tag_index; free_attribute++)
		free(curr_token->attribute[free_attribute]);
	free(curr_token->attribute);

	free(curr_token->max_data);
	free(curr_token->max_attr_tag);
	free(curr_token->max_children);

	if (curr_token->deep_copy) {
		// free children
		for (int free_child = 0; free_child < curr_token->children_index; free_child++)
			destroy_token(curr_token->children[free_child]);
	}
	free(curr_token->children);

	free(curr_token);

	return 0;
}

int yomu_tagMETA(yomu_t *search_token, char *search_tag, char ***found_tag, int *max_tag, int *tag_index) {
	// check if current tree position has correct tag
	if (strcmp(search_token->tag, search_tag) == 0) {
		(*found_tag)[(*tag_index)++] = search_token->data;

		*found_tag = (char **) resize_array(*found_tag, max_tag, *tag_index, sizeof(char *));
	}

	// check children
	for (int check_child = 0; check_child < search_token->children_index; check_child++) {
		yomu_tagMETA(search_token->children[check_child], search_tag, found_tag, max_tag, tag_index);
	}

	return 0;
}

char **token_get_tag_data(yomu_t *search_token, char *tag_name, int *max_tag) {
	// setup char ** array
	*max_tag = 8;
	char **found_tag = malloc(sizeof(char *) * *max_tag);
	int *tag_index = (int *) malloc(sizeof(int));
	*tag_index = 0;

	yomu_tagMETA(search_token, tag_name, &found_tag, max_tag, tag_index);

	// move value into max tag (which becomes the index for the return value)
	*max_tag = *tag_index;
	free(tag_index);

	return found_tag;
}

char *resize_full_data(char *full_data, int *data_max, int data_index) {
	while (data_index >= *data_max) {
		*data_max *= 2;
		full_data = realloc(full_data, sizeof(char) * *data_max);
	}

	return full_data;
}

int read_data_match(match_t *match, int *match_len, yomu_t *yomu) {
	if (!match)
		return 1;

	// look at each match param:
	for (int m = 0; m < *match_len; m++) {
		if (!match[m].match(yomu, match[m].match_tag))
			return 0;
	}

	return 1;
}

int token_read_all_data_helper(yomu_t *search_token, char **full_data, int *data_max, int data_index, match_t *match, int *match_len, int recur) {
	// update reads specifically for %s's first to place sub tabs into the correct places
	int add_from_child = 0;
	
	for (int read_token_data = 0; read_token_data < search_token->data_index; read_token_data++) {
		if (search_token->data[read_token_data] == '<' && (read_token_data < search_token->data_index - 1 &&
			search_token->data[read_token_data + 1] == '>')) {
			if (!read_data_match(match, match_len, search_token->children[add_from_child])) {
				add_from_child++;
				read_token_data++;
				continue;
			}

			if ((search_token->deep_copy + recur == 0) || recur) {
				// make sure the next element can be search:
				while (yomu_f.close_forbidden(yomu_f.forbidden_close_tags, search_token->children[add_from_child]->tag))
					add_from_child++;

				data_index = token_read_all_data_helper(search_token->children[add_from_child],
					full_data, data_max, data_index, match, match_len, recur);

				// move to next child
				add_from_child++;
			}

			// skip other process
			read_token_data++;
			continue;
		}

		// check full_data has enough space
		*full_data = resize_full_data(*full_data, data_max, data_index + 1);

		// add next character
		(*full_data)[data_index] = search_token->data[read_token_data];
		data_index++;
	}

	return data_index;
}

// go through the entire sub tree and create a char * of all data values
char *token_read_all_data(yomu_t *search_token, int *data_max, match_t *match, int *match_len, int recur) {
	int data_index = 0;
	*data_max = 8;
	char **full_data = malloc(sizeof(char *));
	*full_data = malloc(sizeof(char) * *data_max);

	data_index = token_read_all_data_helper(search_token, full_data, data_max, 0, match, match_len, recur);

	(*full_data)[data_index] = 0;

	*data_max = data_index;

	char *pull_data = *full_data;
	free(full_data);
	return pull_data;
}

char *read_token(yomu_t *search_token, char *option, ...) {
	int deep_read = 1;

	va_list opt_list;
	va_start(opt_list, option);

	int *match_len = malloc(sizeof(int));
	match_t *match = NULL;

	for (int o = 0; option[o]; o++) {
		if (option[o] != '-')
			continue;

		if (option[o + 1] == 's')
			deep_read = 0;
		else if (option[o + 1] == 'm')
			match = create_matches(va_arg(opt_list, char *), match_len);
	}

	int *data_len = malloc(sizeof(int));
	char *data = token_read_all_data(search_token, data_len, match, match_len, deep_read);

	free(data_len);

	free(match_len);
	if (match)
		free(match);

	return data;
}

int update_token(yomu_t *search_token, char *data) {
	return 0;
}

int has_attr_value(char **attribute, int attr_len, char *attr, char *attr_value) {
	int attr_pos;
	for (attr_pos = 0; attr_pos < attr_len; attr_pos += 2) {
		if (strcmp(attribute[attr_pos], attr) == 0)
			break;
	}

	if (attr_pos == attr_len)
		return 0;

	attr_pos++;

	int *classlist_len = malloc(sizeof(int));
	char **classlist = split_string(attribute[attr_pos], ' ', classlist_len, "-c");

	int found_class = 0;
	for (int check_classlist = 0; check_classlist < *classlist_len; check_classlist++) {
		if (strcmp(classlist[check_classlist], attr_value) == 0)
			found_class = 1;

		free(classlist[check_classlist]);
	}

	free(classlist);
	free(classlist_len);

	return found_class;
}

int token_has_classname(yomu_t *token, char *classname) {
	return has_attr_value(token->attribute, token->attr_tag_index, "class", classname);
}

// takes current search token and searches for the attribute `class=classname`
// will return first occurrence of a token that has that classname
yomu_t *grab_token_by_classname(yomu_t *start_token, char *classname) {
	// if it doesn't exists, return NULL
	for (int check_children = 0; check_children < start_token->children_index; check_children++) {
		// compare tag:
		if (has_attr_value(start_token->children[check_children]->attribute,
			start_token->children[check_children]->attr_tag_index, "class", classname))
			return start_token->children[check_children];
	}

	// otherwise check children
	for (int bfs_children = 0; bfs_children < start_token->children_index; bfs_children++) {
		yomu_t *check_children_token = grab_token_by_classname(start_token->children[bfs_children], classname);

		if (check_children_token)
			return check_children_token;
	}

	return NULL;
}

int read_main_tag(char **main_tag, char *curr_line, int search_tag) {
	*main_tag = malloc(sizeof(char) * 8);
	int max_main_tag = 8, main_tag_index = 0;

	search_tag++;

	while (curr_line[search_tag] != '>' &&
		   curr_line[search_tag] != '\n' &&
		   curr_line[search_tag] != ' ') {

		(*main_tag)[main_tag_index] = curr_line[search_tag];

		search_tag++;
		main_tag_index++;

		*main_tag = resize_array(*main_tag, &max_main_tag, main_tag_index, sizeof(char));

		(*main_tag)[main_tag_index] = '\0';
	}

	return search_tag;
}

int read_newline(char **curr_line, size_t *buffer_size, FILE *fp, char *str_read) {
	if (fp)
		return getdelim(curr_line, buffer_size, 10, fp);

	// otherwise search through str_read for a newline
	int str_read_index = 0;

	while (str_read[0] != '\0') {
		if (str_read[0] == '\n')
			break;

		(*curr_line)[str_read_index] = str_read[0];

		str_read += sizeof(char);
		str_read_index++;

		// check for resize
		if ((str_read_index + 1) * sizeof(char) == *buffer_size) {
			// increase
			*buffer_size *= 2;
			*curr_line = realloc(*curr_line, *buffer_size);
		}

		(*curr_line)[str_read_index] = '\0';
	}

	if (str_read[0] == '\0' && str_read_index == 0)
		return -1;

	(*curr_line)[str_read_index] = '\n';
	(*curr_line)[str_read_index + 1] = '\0';

	return str_read_index + (str_read[0] != '\0');
}

int find_close_tag(FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_close) {
	while((*curr_line)[search_close] != '>') {
		if ((*curr_line)[search_close] == '\n') {
			search_close = 0;

			if (!file)
				str_read += (search_close + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			continue;
		}

		search_close++;
	}

	return search_close;
}

char *read_close_tag(FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_close) {
	int *max_close_tag = malloc(sizeof(int)), close_tag_index = 0;
	*max_close_tag = 8;
	char *close_tag = malloc(sizeof(char) * 8);

	while((*curr_line)[search_close] != '>') {
		if ((*curr_line)[search_close] == '\n') {
			search_close = 0;

			if (!file)
				str_read += (search_close + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			continue;
		}

		close_tag[close_tag_index++] = (*curr_line)[search_close];
		close_tag = resize_array(close_tag, max_close_tag, close_tag_index, sizeof(char));
		close_tag[close_tag_index] = '\0';

		search_close++;
	}

	free(max_close_tag);

	return close_tag;
}

typedef struct TagRead {
	char *update_str_read;
	int new_search_token;
	int type; // 0 for normal (<div></div>), 1 for closure (<img/>), 2 for comment (<!---->)
} tag_reader;

tag_reader find_end_comment(FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_token) {

	tag_reader tag_read = { .new_search_token = search_token, .type = 2 };

	while (1) {
		if ((*curr_line)[search_token] == '-' && (*curr_line)[search_token + 1] == '-' && (*curr_line)[search_token + 2] == '>')
			break;

		if ((int) (*curr_line)[search_token] == 10) {
			search_token = 0;

			if (!file)
				str_read += (search_token + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			continue;
		}

		search_token++;
	}

	tag_read.new_search_token = search_token + 3;
	tag_read.update_str_read = str_read;

	return tag_read;
}

// builds a new tree and adds it as a child of parent_tree
tag_reader read_tag(yomu_t *parent_tree, FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_token) {
	// check for comment:
	if ((*curr_line)[search_token + 1] == '!' && (*curr_line)[search_token + 2] == '-' && (*curr_line)[search_token + 3] == '-')
		return find_end_comment(file, str_read, curr_line, buffer_size, search_token);

	char *main_tag;
	search_token = read_main_tag(&main_tag, *curr_line, search_token);

	yomu_t *new_tree = create_token(main_tag, parent_tree, 1);

	int read_tag = 1; // choose whether to add to attr_tag_name
					  // or attr_tag_value: 1 to add to attr_tag_name
					  // 0 to add to attr_tag_value
	int start_attr_value = 0; // checks if reading the attribute has begun
	/*
		primarily for when reading the attribute value:
		tag="hello"

		only after the first " is seen should reading the value occur
	*/

	char *attr_tag_name = malloc(sizeof(char) * 8);
	int max_attr_tag_name = 8, attr_tag_name_index = 0;
	memset(attr_tag_name, '\0', 8);
	char *attr_tag_value = malloc(sizeof(char) * 8);

	int attr_tag_value_enclose_type = 0; // choose if the attribute value
		// is wrapped in "" or '', 0 for "", 1 for ''
	int max_attr_tag_value = 8, attr_tag_value_index = 0;
	memset(attr_tag_value, '\0', 8);

	tag_reader tag_read;

	// searching for attributes
	while ((*curr_line)[search_token] != '>') {

		if ((*curr_line)[search_token] == '\n') {
			if (!file)
				str_read += (search_token + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			search_token = 0;
		}

		if (read_tag) {
			if ((*curr_line)[search_token] == '=') { // switch to reading value
				read_tag = 0;

				attr_tag_value_index = 0;

				search_token++;
				continue;
			} else if (attr_tag_name_index > 0 && (*curr_line)[search_token] != ' ' &&
				(*curr_line)[search_token - 1] == ' ') {
				// this means there was no attr value, so add this
				// with a NULL value and move into the next one
				char *tag_name = malloc(sizeof(char) * (attr_tag_name_index + 1));
				strcpy(tag_name, attr_tag_name);

				add_token_attribute(new_tree, tag_name, NULL);

				attr_tag_name_index = 0;
				continue;
			}

			if ((*curr_line)[search_token] != ' ') {
				attr_tag_name[attr_tag_name_index++] = (*curr_line)[search_token];

				attr_tag_name = resize_array(attr_tag_name, &max_attr_tag_name, attr_tag_name_index, sizeof(char));
				attr_tag_name[attr_tag_name_index] = '\0';
			}
		} else {
			if ((!attr_tag_value_enclose_type && (*curr_line)[search_token] == '"') ||
				(attr_tag_value_enclose_type && (*curr_line)[search_token] == '\'')) {
				if (start_attr_value) {
					// add to the token
					char *tag_name = malloc(sizeof(char) * (attr_tag_name_index + 1));
					strcpy(tag_name, attr_tag_name);

					char *attr = malloc(sizeof(char) * (attr_tag_value_index + 1));
					strcpy(attr, attr_tag_value);
					add_token_attribute(new_tree, tag_name, attr);

					attr_tag_name_index = 0;
					attr_tag_value_index = 0;

					read_tag = 1;
					start_attr_value = 0;

					memset(attr_tag_value, '\0', max_attr_tag_value * sizeof(char));
					memset(attr_tag_name, '\0', max_attr_tag_name * sizeof(char));
				} else {
					start_attr_value = 1;
					attr_tag_value_enclose_type = (*curr_line)[search_token] == '\'';
				}

				search_token++;
				continue;
			}

			if (!start_attr_value) {
				search_token++;
				continue;
			}

			attr_tag_value[attr_tag_value_index++] = (*curr_line)[search_token];

			attr_tag_value = resize_array(attr_tag_value, &max_attr_tag_value, attr_tag_value_index, sizeof(char));
			attr_tag_value[attr_tag_value_index] = '\0';
		}

		search_token++;
	}

	if (attr_tag_name_index > 0) {
		char *tag_name = malloc(sizeof(char) * (attr_tag_name_index + 1));
		strcpy(tag_name, attr_tag_name);

		char *attr = NULL;
		if (attr_tag_value_index > 0) {
			attr = malloc(sizeof(char) * (attr_tag_value_index + 1));
			strcpy(attr, attr_tag_value);
		}

		add_token_attribute(new_tree, tag_name, attr);
	}

	free(attr_tag_name);
	free(attr_tag_value);

	add_token_children(parent_tree, new_tree);

	tag_read.new_search_token = search_token + 1;
	tag_read.type = yomu_f.close_forbidden(yomu_f.forbidden_close_tags, main_tag);
	tag_read.update_str_read = str_read;
	return tag_read;
}

int tokenizeMETA(FILE *file, char *str_read, yomu_t *curr_tree) {
	int search_token = 0;

	size_t *buffer_size = malloc(sizeof(size_t));
	*buffer_size = sizeof(char) * 123;
	char **buffer_reader = malloc(sizeof(char *));
	*buffer_reader = malloc(*buffer_size);

	int read_len = 0;
	int line = 0;

	while((read_len = read_newline(buffer_reader, buffer_size, file, str_read)) != -1) {
		// move str_read forward
		str_read += read_len * sizeof(char);

		search_token = 0;
		char *curr_line = *buffer_reader;

		while (curr_line[search_token] != '\n' && curr_line[search_token] != '\0') {
			if (curr_line[search_token] == '<') {
				if (curr_line[search_token + 1] == '/') { // close tag
					// return to parent tree instead

					// check tag name matches curr_tree tag
					curr_tree = grab_token_parent(curr_tree);

					search_token = find_close_tag(file, str_read, buffer_reader, buffer_size, search_token) + 1;

					continue;
				}

				tag_reader tag_read = read_tag(curr_tree, file, str_read, buffer_reader, buffer_size, search_token);
				search_token = tag_read.new_search_token;

				str_read = tag_read.update_str_read;

				// depending on tag_read.type, choose specific path:
				if (tag_read.type == 0) {
					// add pointer to sub tree within data:
					add_token_rolling_data(curr_tree, '<');
					add_token_rolling_data(curr_tree, '>');
					
					curr_tree = grab_token_children(curr_tree);
				}

				continue;
			}

			// otherwise add character to the curr_tree
			add_token_rolling_data(curr_tree, curr_line[search_token]);

			search_token++;
		}

		add_token_rolling_data(curr_tree, '\n');
	}

	free(buffer_size);
	free(buffer_reader[0]);
	free(buffer_reader);

	return 0;
}

// see comment on tokenize for ruleset
// 1 for file
// 0 for html
int file_or_html_name(char *data) {
	for (int check_name = 0; data[check_name]; check_name++) {
		if (data[check_name] == '<' || data[check_name] == '>')
			return 1;
	}

	return 0;
}

hashmap *set_forbidden_close_tags() {
	hashmap *fct = make__hashmap(0, NULL, destroyIntKey);

	int **forbidden_true = malloc(sizeof(int *) * 13);
	for (int i = 0; i < 13; i++) {
		forbidden_true[i] = malloc(sizeof(int));
		*forbidden_true[i] = 1;
	}

	// based on the forbidden close tags described here:
	// https://stackoverflow.com/questions/3008593/html-include-or-exclude-optional-closing-tags
	char *img_f = malloc(sizeof(char) * 4); strcpy(img_f, "img");
	char *input_f = malloc(sizeof(char) * 6); strcpy(input_f, "input");
	char *br_f = malloc(sizeof(char) * 3); strcpy(br_f, "br");
	char *hr_f = malloc(sizeof(char) * 3); strcpy(hr_f, "hr");
	char *frame_f = malloc(sizeof(char) * 6); strcpy(frame_f, "frame");
	char *area_f = malloc(sizeof(char) * 5); strcpy(area_f, "area");
	char *base_f = malloc(sizeof(char) * 5); strcpy(base_f, "base");
	char *basefont_f = malloc(sizeof(char) * 9); strcpy(basefont_f, "basefont");
	char *col_f = malloc(sizeof(char) * 4); strcpy(col_f, "col");
	char *isindex_f = malloc(sizeof(char) * 8); strcpy(isindex_f, "isindex");
	char *link_f = malloc(sizeof(char) * 5); strcpy(link_f, "link");
	char *meta_f = malloc(sizeof(char) * 5); strcpy(meta_f, "meta");
	char *param_f = malloc(sizeof(char) * 6); strcpy(param_f, "param");

	// insert into fct:
	insert__hashmap(fct, img_f, forbidden_true[0], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, input_f, forbidden_true[1], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, br_f, forbidden_true[2], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, hr_f, forbidden_true[3], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, frame_f, forbidden_true[4], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, area_f, forbidden_true[5], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, base_f, forbidden_true[6], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, basefont_f, forbidden_true[7], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, col_f, forbidden_true[8], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, isindex_f, forbidden_true[9], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, link_f, forbidden_true[10], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, meta_f, forbidden_true[11], "", NULL, compareCharKey, destroyCharKey);
	insert__hashmap(fct, param_f, forbidden_true[12], "", NULL, compareCharKey, destroyCharKey);

	free(forbidden_true);

	return fct;
}

/*
	tokenize takes in either a filename or a char * filled with the <HTML> data
	search for a "<" or ">" in the filename since if the filename contains one
	of those, it is thereby an illegal filename (https://www.mtu.edu/umc/services/websites/writing/characters-avoid/)
*/
yomu_t *tokenize(char *filename) {
	int make_file = !file_or_html_name(filename);

	FILE *file = make_file ? fopen(filename, "r") : NULL;

	if (make_file && !file) {
		printf("Uh oh! Something was wrong with the file: %s\n", filename);
		exit(1);
	}

	char *root_tag = malloc(sizeof(char) * 5);
	strcpy(root_tag, "root");
	yomu_t *curr_tree = create_token(root_tag, NULL, 1);

	tokenizeMETA(file, filename, curr_tree);

	if (file)
		fclose(file);

	return curr_tree;
}

int all_match(yomu_t *y, char *any) {
	return 1;
}

int anti_all_match(yomu_t *y, char *any) {
	return 0;
}

int id_match(yomu_t *y, char *id) {
	return has_attr_value(y->attribute, y->attr_tag_index, "id", id);
}

int anti_id_match(yomu_t *y, char *id) {
	return !has_attr_value(y->attribute, y->attr_tag_index, "id", id);
}

int class_match(yomu_t *y, char *class) {
	int class_index;
	for (class_index = 0; class_index < y->attr_tag_index; class_index += 2) {
		if (strcmp(y->attribute[class_index], "class") == 0)
			break;
	}

	if (class_index == y->attr_tag_index) // no class attribute
		return 0;

	// separate all class options
	int *class_list_len = malloc(sizeof(int)), *yomu_class_len = malloc(sizeof(int));
	char **class_list = split_string(class, '.', class_list_len, "-c");
	char **yomu_class = split_string(y->attribute[class_index + 1], ' ', yomu_class_len, "-c");

	// make sure each class in class_list occurs in yomu_class
	for (int check_class_list_match = 0; check_class_list_match < *class_list_len; check_class_list_match++) {

		int find_yomu_class;
		for (find_yomu_class = 0; find_yomu_class < *yomu_class_len; find_yomu_class++) {
			if (strcmp(class_list[check_class_list_match], yomu_class[find_yomu_class]) == 0)
				break;
		}

		// if find_yomu_class == *yomu_class_len, that means there was not an occurrence of
		// the class_list class, so this should return false
		if (find_yomu_class == *yomu_class_len)
			return 0;	
	}

	// otherwise
	return 1;
}

int anti_class_match(yomu_t *y, char *class) {
	int class_index;
	for (class_index = 0; class_index < y->attr_tag_index; class_index += 2) {
		if (strcmp(y->attribute[class_index], "class") == 0)
			break;
	}

	if (class_index == y->attr_tag_index) // no class attribute
		return 1;

	// separate all class options
	int *class_list_len = malloc(sizeof(int)), *yomu_class_len = malloc(sizeof(int));
	char **class_list = split_string(class, '.', class_list_len, "-c");
	char **yomu_class = split_string(y->attribute[class_index + 1], ' ', yomu_class_len, "-c");

	// make sure each class in class_list occurs in yomu_class
	for (int check_class_list_match = 0; check_class_list_match < *class_list_len; check_class_list_match++) {

		int find_yomu_class;
		for (find_yomu_class = 0; find_yomu_class < *yomu_class_len; find_yomu_class++) {
			if (strcmp(class_list[check_class_list_match], yomu_class[find_yomu_class]) == 0)
				break;
		}

		// if find_yomu_class == *yomu_class_len, that means there was not an occurrence of
		// the class_list class, so this should return false
		if (find_yomu_class == *yomu_class_len)
			return 1;	
	}

	// otherwise
	return 0;
}

int tag_match(yomu_t *y, char *tag) {
	return strcmp(y->tag, tag) == 0;
}

int anti_tag_match(yomu_t *y, char *tag) {
	return !(strcmp(y->tag, tag + sizeof(char)) == 0);
}

match_t match_create(int (*match)(yomu_t *, char *), char *match_tag) {
	match_t new_match = {
		.match = match,
		.match_tag = match_tag
	};

	return new_match;
}

/* this will search through the char *match_param to compute all of the that are required
	for example:

		".classname h1"

	will return an array of match_t's:

	[
		class_match,
		tag_match,
		etc.
	]

	so then when running compute_matches, first get the results with the first found match,
	then using the first results, find the sub results of each result and so on and so forth
*/
match_t *create_matches(char *match_param, int *match_param_length) {
	int *sub_match_param_length = malloc(sizeof(int));
	char **sub_match_params = split_string(match_param, ' ', sub_match_param_length, "-c");

	*match_param_length = *sub_match_param_length;
	match_t *return_matches = malloc(sizeof(match_t) * *match_param_length);

	// search through sub_match_params to see which function is needed
	for (int find_match = 0; find_match < *match_param_length; find_match++) {
		int reverse_match = sub_match_params[find_match][0] == '!';		

		// check first value in the char * to decide which type we are searching for:
		if ((reverse_match && sub_match_params[find_match][1] == '*')
			|| sub_match_params[find_match][0] == '*') // match all
			return_matches[find_match] = match_create(reverse_match ? anti_all_match : all_match,
				reverse_match ? sub_match_params[find_match] + sizeof(char) : sub_match_params[find_match]);
		else if ((reverse_match && sub_match_params[find_match][1] == '.')
			|| sub_match_params[find_match][0] == '.') // class
			return_matches[find_match] = match_create(reverse_match ? anti_class_match : class_match,
				reverse_match ? sub_match_params[find_match] + (sizeof(char) * 2) : sub_match_params[find_match] + sizeof(char));
		else if ((reverse_match && sub_match_params[find_match][1] == '#')
			|| sub_match_params[find_match][0] == '#') // id
			return_matches[find_match] = match_create(reverse_match ? anti_id_match : id_match,
				reverse_match ? sub_match_params[find_match] + (sizeof(char) * 2) : sub_match_params[find_match] + sizeof(char));
		else // tag
			return_matches[find_match] = match_create(reverse_match ? anti_tag_match : tag_match, sub_match_params[find_match]);
	}

	return return_matches;
}

yomu_t **compute_matches(yomu_t *y, char *match_param, int *length, int depth) {
	int *match_param_length = malloc(sizeof(int));
	// compute matches
	match_t *matches = create_matches(match_param, match_param_length);

	if (*match_param_length == 0) // no matches?
		return NULL;

	// for each match:
	int *yomu_len = malloc(sizeof(int)), *yomu_prev_len = malloc(sizeof(int)), *yomu_buffer_len = malloc(sizeof(int));
	yomu_t **yomu_match, **yomu_prev = NULL, **yomu_buffer;
	yomu_match = grab_tokens_by_match(y, matches[0].match, matches[0].match_tag, yomu_len, depth);
	for (int find_match = 1; depth && find_match < *match_param_length; find_match++) {
		if (yomu_prev)
			free(yomu_prev);
		*yomu_prev_len = *yomu_len;
		yomu_prev = yomu_match;

		// setup up a reader for each token in yomu_prev
		*yomu_len = 8;
		int yomu_index = 0;
		for (int read_prev = 0; read_prev < *yomu_prev_len; read_prev++) {
			yomu_buffer = grab_tokens_by_match(yomu_prev[read_prev], matches[find_match].match, matches[find_match].match_tag, yomu_buffer_len, depth);

			if (yomu_index >= *yomu_len) {
				*yomu_len *= 2;
				yomu_match = realloc(yomu_match, sizeof(yomu_t *) * *yomu_len);
			}

			for (int copy_buffer_to_match = 0; copy_buffer_to_match < *yomu_buffer_len; copy_buffer_to_match++) {
				yomu_match[yomu_index] = yomu_buffer[copy_buffer_to_match];
				yomu_index++;
			}
		}

		*yomu_len = yomu_index;
	}

	// copy yomu_len in length
	if (length)
		*length = *yomu_len;

	// free all allocations
	free(match_param_length);
	free(matches);

	free(yomu_len);
	free(yomu_prev_len);
	free(yomu_buffer_len);

	return yomu_match;
}

yomu_t **compute_match_shallow(yomu_t *y, char *match_param, int *length) {
	return compute_matches(y, match_param, length, 0);
}

yomu_t **compute_match_deep(yomu_t *y, char *match_param, int *length) {
	return compute_matches(y, match_param, length, 1);
}

yomu_t *compute_match(yomu_t *y, char *match_param, int direction) {
	int *match_param_length = malloc(sizeof(int));
	// compute matches
	match_t *matches = create_matches(match_param, match_param_length);

	if (*match_param_length == 0) // no matches?
		return NULL;

	yomu_t *yomu_match = grab_token_by_match(y, matches[0].match, matches[0].match_tag, direction);

	// free matches (in case there are many):
	for (int free_match = 0; free_match < *match_param_length; free_match++) {
		free(matches[free_match].match_tag);
	}

	free(match_param_length);
	free(matches);

	return yomu_match;
}

yomu_t *yomu_merge(int y_len, yomu_t **y) {
	yomu_t *y_cp = create_token("root", NULL, 0);

	*y_cp->max_children = (y_len + 1);
	y_cp->children_index = y_len;

	y_cp->children = realloc(y_cp->children, sizeof(yomu_t *) * *y_cp->max_children);

	*y_cp->max_data += (y_len + 1) * 2 + 1;

	y_cp->data = realloc(y_cp->data, sizeof(char) * *y_cp->max_data);

	for (int update_children = 0; update_children < y_len; update_children++) {
		if (!yomu_f.close_forbidden(yomu_f.forbidden_close_tags, y[update_children]->tag)) {
			strcat(y_cp->data, "<>");
			y_cp->data_index += 2;
		}

		y_cp->children[update_children] = y[update_children];
	}

	return y_cp;
}

/*
	searches for first occurence of a match starting with the children of y
	direction: defines if the search starts at the end or the beginning:
	0 for beginning
	1 for end
*/
yomu_t *grab_first_token_by_match(yomu_t *y, char *match_param) {
	return compute_match(y, match_param, 0);
}

yomu_t *grab_last_token_by_match(yomu_t *y, char *match_param) {
	return compute_match(y, match_param, 1);
}

int close_forbidden(hashmap *fct, char *tag_check) {
	int *t = get__hashmap(fct, tag_check);

	return t ? 1 : 0;
}

void yomu_initialize() {
	yomu_f.forbidden_close_tags = set_forbidden_close_tags();
}

void yomu_finalize() {
	deepdestroy__hashmap(yomu_f.forbidden_close_tags);
}

yomu_func_t yomu_f = (yomu_func_t){
	.forbidden_close_tags = NULL,
	.close_forbidden = close_forbidden,

	.parse = tokenize,

	.children = compute_match_shallow,
	.find = compute_match_deep,
	.first = grab_first_token_by_match,
	.last = grab_last_token_by_match,

	.merge = yomu_merge,

	.parent = grab_token_parent,

	.attr = {
		.set = set_attr,
		.get = token_attr
	},

	.hasClass = token_has_classname,

	.update = update_token,
	.read = read_token,

	.destroy = destroy_token,

	.init = yomu_initialize,
	.close = yomu_finalize
};

/* HELPER FUNCTIONS */
int is_range(char c) {
	return 1;
}

int delimeter_check(char curr_char, char *delims) {
	for (int check_delim = 0; delims[check_delim]; check_delim++) {
		if (delims[check_delim] == curr_char)
			return 1;
	}

	return 0;
}

char **split_string(char *full_string, char delimeter, int *arr_len, char *extra, ...) {
	va_list param;
	va_start(param, extra);

	int **minor_length = NULL;
	int (*is_delim)(char, char *) = NULL;
	char *multi_delims = NULL;

	int (*f_is_range)(char) = is_range;

	int should_lowercase = 1;

	for (int check_extra = 0; extra[check_extra]; check_extra++) {
		if (extra[check_extra] != '-')
			continue;

		if (extra[check_extra + 1] == 'l')
			minor_length = va_arg(param, int **);
		else if (extra[check_extra + 1] == 'd') {
			is_delim = va_arg(param, int (*)(char, char *));
			multi_delims = va_arg(param, char *);
		} else if (extra[check_extra + 1] == 'r') {
			f_is_range = va_arg(param, int (*)(char));
		} else if (extra[check_extra + 1] == 'c')
			should_lowercase = 0;
	}

	int arr_index = 0;
	*arr_len = 8;
	char **arr = malloc(sizeof(char *) * *arr_len);

	if (minor_length)
		*minor_length = realloc(*minor_length, sizeof(int) * *arr_len);

	int *max_curr_sub_word = malloc(sizeof(int)), curr_sub_word_index = 0;
	*max_curr_sub_word = 8;
	arr[arr_index] = malloc(sizeof(char) * *max_curr_sub_word);
	arr[arr_index][0] = '\0';

	for (int read_string = 0; full_string[read_string]; read_string++) {
		if ((is_delim && is_delim(full_string[read_string], multi_delims)) || full_string[read_string] == delimeter) { // next phrase
			// check that current word has some length
			if (arr[arr_index][0] == '\0')
				continue;

			// quickly copy curr_sub_word_index into minor_length if minor_length is defined:
			if (minor_length) {
				(*minor_length)[arr_index] = curr_sub_word_index;
			}

			arr_index++;

			while (arr_index >= *arr_len) {
				*arr_len *= 2;
				arr = realloc(arr, sizeof(char *) * *arr_len);

				if (minor_length)
					*minor_length = realloc(*minor_length, sizeof(int *) * *arr_len);
			}

			curr_sub_word_index = 0;
			*max_curr_sub_word = 8;
			arr[arr_index] = malloc(sizeof(char) * *max_curr_sub_word);
			arr[arr_index][0] = '\0';

			continue;
		}

		// if not in range, skip:
		if (f_is_range && !f_is_range(full_string[read_string]))
			continue;

		// if a capital letter, lowercase
		if (should_lowercase && (int) full_string[read_string] <= 90 && (int) full_string[read_string] >= 65)
			full_string[read_string] = (char) ((int) full_string[read_string] + 32);

		arr[arr_index][curr_sub_word_index] = full_string[read_string];
		curr_sub_word_index++;

		arr[arr_index] = resize_array(arr[arr_index], max_curr_sub_word, curr_sub_word_index, sizeof(char));

		arr[arr_index][curr_sub_word_index] = '\0';
	}

	if (arr[arr_index][0] == '\0') { // free position
		free(arr[arr_index]);

		arr_index--;
	} else if (minor_length)
			(*minor_length)[arr_index] = curr_sub_word_index;

	*arr_len = arr_index + 1;

	free(max_curr_sub_word);

	return arr;
}

/*
 _               _                           
| |__   __ _ ___| |__  _ __ ___   __ _ _ __  
| '_ \ / _` / __| '_ \| '_ ` _ \ / _` | '_ \ 
| | | | (_| \__ \ | | | | | | | | (_| | |_) |
|_| |_|\__,_|___/_| |_|_| |_| |_|\__,_| .__/ 
                                      |_|
*/
unsigned long hash(unsigned char *str) {
	unsigned long hash = 5381;
	int c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c;

	return hash;
}


// define some linked list functions (see bottom of file for function write outs):
ll_main_t *ll_makeNode(vtableKeyStore key, void *value, int hash__type);
int ll_insert(ll_main_t *node, vtableKeyStore key, void *payload, int hash__type, void (*destroy)(void *));

ll_main_t *ll_next(ll_main_t *curr);

int ll_print(ll_main_t *curr, void (*printer)(void *));

int ll_isolate(ll_main_t *node);
int ll_destroy(ll_main_t *node, void (destroyObjectPayload)(void *));


hashmap *make__hashmap(int hash__type, void (*printer)(void *), void (*destroy)(void *)) {
	hashmap *newMap = (hashmap *) malloc(sizeof(hashmap));

	newMap->hash__type = hash__type;
	newMap->hashmap__size = START_SIZE;

	// define needed input functions
	newMap->printer = printer;
	newMap->destroy = destroy;

	newMap->map = (ll_main_t **) malloc(sizeof(ll_main_t *) * newMap->hashmap__size);

	for (int i = 0; i < newMap->hashmap__size; i++) {
		newMap->map[i] = NULL;
	}

	return newMap;
}

// take a previous hashmap and double the size of the array
// this means we have to take all the inputs and calculate
// a new position in the new array size
// within hash__m->map we can access each of the linked list pointer
// and redirect them since we store the keys
int re__hashmap(hashmap *hash__m) {
	// define the sizes / updates to size:
	int old__mapLength = hash__m->hashmap__size;
	int new__mapLength = hash__m->hashmap__size * 2;

	// create new map (twice the size of old map (AKA hash__m->map))
	ll_main_t **new__map = (ll_main_t **) malloc(sizeof(ll_main_t *) * new__mapLength);

	for (int set__newMapNulls = 0; set__newMapNulls < new__mapLength; set__newMapNulls++)
		new__map[set__newMapNulls] = NULL;

	int new__mapPos = 0;

	for (int old__mapPos = 0; old__mapPos < old__mapLength; old__mapPos++) {
		// look at each bucket
		// if there is contents
		while (hash__m->map[old__mapPos]) { // need to look at each linked node
			// recalculate hash
			new__mapPos = hash(hash__m->map[old__mapPos]->key.key) % new__mapLength;

			// store the node in temporary storage
			ll_main_t *currNode = hash__m->map[old__mapPos];

			// extract currNode from old map (hash__m->map)
			hash__m->map[old__mapPos] = ll_next(currNode); // advance root
			ll_isolate(currNode); // isolate old root

			// defines the linked list head in the new map
			ll_main_t *new__mapBucketPtr = new__map[new__mapPos];

			// if there is one, we have to go to the tail
			if (new__mapBucketPtr) {

				while (new__mapBucketPtr->next) new__mapBucketPtr = ll_next(new__mapBucketPtr);

				new__mapBucketPtr->next = currNode;
			} else
				new__map[new__mapPos] = currNode;
		}
	}

	free(hash__m->map);
	hash__m->map = new__map;
	hash__m->hashmap__size = new__mapLength;

	return 0;
}

int METAinsert__hashmap(hashmap *hash__m, vtableKeyStore key, void *value) {
	int mapPos = hash(key.key) % hash__m->hashmap__size;
	int bucketLength = 0; // counts size of the bucket at mapPos

	// see if there is already a bucket defined at mapPos
	if (hash__m->map[mapPos])
		bucketLength = ll_insert(hash__m->map[mapPos], key, value, hash__m->hash__type, hash__m->destroy);
	else
		hash__m->map[mapPos] = ll_makeNode(key, value, hash__m->hash__type);

	// if bucketLength is greater than an arbitrary amount,
	// need to grow the size of the hashmap (doubling)
	if (bucketLength >= MAX_BUCKET_SIZE)
		re__hashmap(hash__m);

	return 0;
}

int ll_get_keys(ll_main_t *ll_node, void **keys, int *max_key, int key_index) {
	while(ll_node) {
		keys[key_index++] = ll_node->key.key;

		if (key_index == *max_key) { // resize
			*max_key *= 2;

			keys = realloc(keys, sizeof(void *) * *max_key);
		}

		ll_node = ll_node->next;
	}

	return key_index;
}

// creates an array of all keys in the hash map
void **keys__hashmap(hashmap *hash__m, int *max_key) {
	int key_index = 0;
	*max_key = 16;
	void **keys = malloc(sizeof(void *) * *max_key);

	for (int find_keys = 0; find_keys < hash__m->hashmap__size; find_keys++) {
		if (hash__m->map[find_keys]) {
			// search LL:
			key_index = ll_get_keys(hash__m->map[find_keys], keys, max_key, key_index);
		}
	}

	*max_key = key_index;

	return keys;
}

/*
	get__hashmap search through a bucket for the inputted key
	the response varies widely based on hash__type

	-- for all of them: NULL is return if the key is not found

		TYPE 0:
			Returns the single value that is found
		TYPE 1:
			Returns a pointer to a struct (hashmap__response) with an array that can be
			searched through and a length of said array.
			This array should be used with caution because accidental
			free()ing of this array will leave the key to that array
			pointing to unknown memory. However, the freeing of the
			returned struct will be left to the user
*/
void *get__hashmap(hashmap *hash__m, void *key) {
	// get hash position
	int mapPos = hash(key) % hash__m->hashmap__size;

	ll_main_t *ll_search = hash__m->map[mapPos];
	// search through the bucket to find any keys that match
	while (ll_search) {
		if (ll_search->key.compareKey(ll_search->key.key, key)) { // found a match

			// depending on the type and mode, this will just return
			// the value:
			if (hash__m->hash__type == 0)
				return ll_search->ll_meat;
			else {
				hashmap__response *returnMeat = malloc(sizeof(hashmap__response));

				if (ll_search->isArray) {
					returnMeat->payload = ll_search->ll_meat;
					returnMeat->payload__length = ll_search->arrIndex + 1;
				} else { // define array
					ll_search->isArray = 1;
					void *ll_tempMeatStorage = ll_search->ll_meat;

					ll_search->max__arrLength = 2;
					ll_search->arrIndex = 0;

					ll_search->ll_meat = malloc(sizeof(void *) * ll_search->max__arrLength * 2);
					((void **) ll_search->ll_meat)[0] = ll_tempMeatStorage;

					returnMeat->payload = ll_tempMeatStorage;
					returnMeat->payload__length = ll_search->arrIndex + 1;
				}

				return returnMeat;
			}
		}

		ll_search = ll_next(ll_search);
	}

	// no key found
	return NULL;
}

int print__hashmap(hashmap *hash__m) {
	for (int i = 0; i < hash__m->hashmap__size; i++) {
		if (hash__m->map[i]) {
			printf("Linked list at index %d ", i);
			ll_print(hash__m->map[i], hash__m->printer);
			printf("\n");
		}
	}
}

// uses the same process as get__hashmap, but deletes the result
// instead. Unfortunately, the get__hashmap function cannot be
// utilized in this context because when the linked list node
// is being extracted, we need to know what the parent of
// the node is
int delete__hashmap(hashmap *hash__m, void *key) {
	// get hash position
	int mapPos = hash(key) % hash__m->hashmap__size;

	ll_main_t *ll_parent = hash__m->map[mapPos];
	ll_main_t *ll_search = ll_next(ll_parent);

	// check parent then move into children nodes in linked list
	if (ll_parent->key.compareKey(ll_parent->key.key, key)) {
		// extract parent from the hashmap:
		hash__m->map[mapPos] = ll_search;

		// ensure entire cut from current ll:
		ll_parent->next = NULL;
		ll_destroy(ll_parent, hash__m->destroy);

		return 0;
	}

	// search through the bucket to find any keys that match
	while (ll_search) {
		if (ll_search->key.compareKey(ll_search->key.key, key)) { // found a match

			// we can then delete the key using the same approach as above
			// extract the key from the linked list
			ll_parent->next = ll_next(ll_search);

			ll_search->next = NULL;
			ll_destroy(ll_search, hash__m->destroy);

			return 0;
		}

		ll_parent = ll_search;
		ll_search = ll_next(ll_search);
	}

	return 0;
}

int deepdestroy__hashmap(hashmap *hash) {
	// destroy linked list children
	for (int i = 0; i < hash->hashmap__size; i++) {
		if (hash->map[i]) {
			ll_destroy(hash->map[i], hash->destroy);
		}
	}

	// destroy map
	free(hash->map);
	free(hash);

	return 0;
}


ll_main_t *ll_makeNode(vtableKeyStore key, void *newValue, int hash__type) {
	ll_main_t *new__node = (ll_main_t *) malloc(sizeof(ll_main_t));

	new__node->isArray = 0;
	new__node->next = NULL;
	new__node->key = key;
	new__node->ll_meat = newValue;

	return new__node;
}

/*
	for hash__type = 0
		takes a linked list node value ptr
		and replaces the value with
		updated value
*/
void *ll_specialUpdateIgnore(void *ll_oldVal, void *newValue, void (*destroy)(void *)) {
	// clean up previous info at this pointer
	destroy(ll_oldVal);

	// update
	return newValue;
}

// takes the ll_pointer->ll_meat and doubles
// size of current array
int ll_resizeArray(ll_main_t *ll_pointer) {
	// declare new array
	void **new__arrayPtr = malloc(sizeof(void *) * ll_pointer->max__arrLength * 2);

	// copy values over
	for (int copyVal = 0; copyVal < ll_pointer->max__arrLength; copyVal++) {
		new__arrayPtr[copyVal] = (void *) ((void **) ll_pointer->ll_meat)[copyVal];
	}

	// free old array pointer
	free(ll_pointer->ll_meat);

	// update to new meat
	ll_pointer->ll_meat = new__arrayPtr;

	return 0;
}

/*
	for hash__type = 1
		takes linked list pointer
		- first makes sure that ll_pointer->ll_meat
		is an array, if not it creates and array
		- then appends value to end of array
*/
int ll_specialUpdateArray(ll_main_t *ll_pointer, void *newValue) {
	// The same "leave key be" for update works here as with ignore

	if (!ll_pointer->isArray) { // need to create an array
		void *ll_tempMeatStorage = ll_pointer->ll_meat;

		// update settings for this pointer
		ll_pointer->max__arrLength = 8;
		ll_pointer->arrIndex = 0;
		ll_pointer->isArray = 1;

		ll_pointer->ll_meat = (void **) malloc(sizeof(void *) * ll_pointer->max__arrLength);
		((void **) (ll_pointer->ll_meat))[0] = ll_tempMeatStorage;

		for (int memSet = 1; memSet < ll_pointer->arrIndex; memSet++)
			((void **) (ll_pointer->ll_meat))[memSet] = NULL;
	}

	// add new value
	ll_pointer->arrIndex++;
	((void **) (ll_pointer->ll_meat))[ll_pointer->arrIndex] = newValue;

	if (ll_pointer->arrIndex == ll_pointer->max__arrLength - 1)
		ll_resizeArray(ll_pointer);

	return 0;
}

// finds the tail and appends
int ll_insert(ll_main_t *crawler__node, vtableKeyStore key, void *newValue, int hash__type, void (*destroy)(void *)) {

	int bucket_size = 1, addedPayload = 0;

	// search through the entire bucket
	// (each node in this linked list)
	while (crawler__node->next) {
		// found a duplicate (only matters
		// for hash__type == 0 or 1)
		if (crawler__node->key.compareKey(crawler__node->key.key, key.key)) {
			if (hash__type == 0) {
				crawler__node->ll_meat = ll_specialUpdateIgnore(crawler__node->ll_meat, newValue, destroy);
				addedPayload = 1;
			} else if (hash__type == 1) {
				ll_specialUpdateArray(crawler__node, newValue);
				addedPayload = 1;
			}
		}

		crawler__node = ll_next(crawler__node);
		bucket_size++;
	}

	printf("Testing keys %d\n", crawler__node->key.compareKey(crawler__node->key.key, key.key));
	if (crawler__node->key.compareKey(crawler__node->key.key, key.key)) {
		if (hash__type == 0) {
			crawler__node->ll_meat = ll_specialUpdateIgnore(crawler__node->ll_meat, newValue, destroy);
			addedPayload = 1;
		} else if (hash__type == 1) {
			ll_specialUpdateArray(crawler__node, newValue);
			addedPayload = 1;
		}
	}

	if (addedPayload == 0) {
		crawler__node->next = ll_makeNode(key, newValue, hash__type);
	}

	// return same head
	return bucket_size;
}

ll_main_t *ll_next(ll_main_t *curr) {
	return curr->next;
}

int ll_printNodeArray(ll_main_t *curr, void (*printer)(void *)) {
	for (int printVal = 0; printVal < curr->arrIndex + 1; printVal++) {
		printer(((void **) curr->ll_meat)[printVal]);
	}

	return 0;
}

int ll_print(ll_main_t *curr, void (*printer)(void *)) {
	printf("\n\tLL node ");
	//printVoid()
	curr->key.printKey(curr->key.key);
	printf(" with payload(s):\n");
	if (curr->isArray)
		ll_printNodeArray(curr, printer);
	else
		printer(curr->ll_meat);

	while (curr = ll_next(curr)) {
		printf("\tLL node ");
		printf(" with payload(s):\n");
		if (curr->isArray)
			ll_printNodeArray(curr, printer);
		else
			printer(curr->ll_meat);
	}

	return 0;
}

int ll_isolate(ll_main_t *node) {
	node->next = NULL;

	return 0;
}

int ll_destroy(ll_main_t *node, void (destroyObjectPayload)(void *)) {
	ll_main_t *node_nextStore;

	do {
		if (node->key.destroyKey)
			node->key.destroyKey(node->key.key);

		if (node->isArray) {
			for (int destroyVal = 0; destroyVal < node->arrIndex + 1; destroyVal++)
				destroyObjectPayload(((void **)node->ll_meat)[destroyVal]);

			free(node->ll_meat);
		} else
			destroyObjectPayload(node->ll_meat);

		node_nextStore = node->next;
		free(node);
	} while (node = node_nextStore);

	return 0;
}


// DEFAULT function declarations
void printCharKey(void *characters) {
	printf("%s", (char *) characters);
}
int compareCharKey(void *characters, void *otherValue) {
	return strcmp((char *) characters, (char *) otherValue) == 0;
}
void destroyCharKey(void *characters) { free(characters); }

void printIntKey(void *integer) {
	printf("%d", *((int *) integer));
}
int compareIntKey(void *integer, void *otherValue) {
	return *((int *) integer) == *((int *) otherValue);
}
void destroyIntKey(void *integer) { free(integer); }


int insert__hashmap(hashmap *hash__m, void *key, void *value, ...) {
	va_list ap;
	vtableKeyStore inserter = { .key = key };

	va_start(ap, value);
	// could be param definer or the printKey function
	void *firstArgumentCheck = va_arg(ap, char *);

	// check for DEFAULT ("-d") or INT ("-i"):
	if (strcmp((char *) firstArgumentCheck, "-d") == 0) {// use default
		inserter.printKey = printCharKey;
		inserter.compareKey = compareCharKey;
		inserter.destroyKey = NULL;
	} else if (strcmp((char *) firstArgumentCheck, "-i") == 0) {
		inserter.printKey = printIntKey;
		inserter.compareKey = compareIntKey;
		inserter.destroyKey = NULL;
	} else {
		inserter.printKey = va_arg(ap, void (*)(void *));
		// do the same for compareKey 
		inserter.compareKey = va_arg(ap, int (*)(void *, void *));

		inserter.destroyKey = va_arg(ap, void (*)(void *));
		// if destroy is NULL, we don't want to add since this by DEFAULT
		// does no exist
	}	

	METAinsert__hashmap(hash__m, inserter, value);

	return 0;
}