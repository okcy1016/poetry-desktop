#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <json-c/json.h>

struct poem {
  const char* title;
  const char* author;
  const char* paragraphs;
};

int loop_poem_and_insert(json_object*, const char*);
int read_poem_2_db(const char*, const char*);
int insert_single_poem(sqlite3*, struct poem*);
static int callback(void*, int, char**, char**);
char* build_insert_sql(struct poem*);

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
  int i;
  for(i=0; i<argc; i++){
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

int read_poem_2_db(const char* jsonpath, const char* dbpath) {
  /* char* is a mutable pointer to a mutable character/string. */

  /* const char* is a mutable pointer to an immutable character/string. */
  /* You cannot change the contents of the location(s) this pointer points to. */
  /* Also, compilers are required to give error messages when you try to do so. */
  /* For the same reason, conversion from const char * to char* is deprecated. */
  
  /* char* const is an immutable pointer (it cannot point to any other location) */
  /* but the contents of location at which it points are mutable. */

  /* const char* const is an immutable pointer to an immutable character/string. */    
  json_object* root = json_object_from_file(jsonpath);

  int poem_count = loop_poem_and_insert(root, dbpath);
  
  json_object_put(root);
  return poem_count;
}

int loop_poem_and_insert(json_object* root, const char* db_path) {
  sqlite3* db;
  int poem_count;
  int para_count;
  json_object* poem_elem;
  json_object* tmp_elem;
  json_object* para_elem;
  struct poem* poem = malloc(sizeof(struct poem));
  // must be initialized, or `\377\377\377` may appear
  // an uninitialized character array may preforms an out of bounds access
  // see https://stackoverflow.com/questions/43384991/different-ways-to-initialize-char-array-are-they-correct
  // fixme: may encounter segment fault if the buffer size is too small
  char buf[1024*32] = {0};

  poem_count = json_object_array_length(root);

  // open db
  int rc = sqlite3_open(db_path, &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // create `poem` table
  const char sql[256] = "CREATE TABLE poem (title string, author string, paragraphs string);";
  sqlite3_exec(db, sql, callback, 0, NULL);

  // wrap in one transaction, speed up bulk insert!
  // see https://stackoverflow.com/questions/1711631/improve-insert-per-second-performance-of-sqlite
  sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
  
  for(int i=0; i<poem_count; i++) {
    poem_elem = json_object_array_get_idx(root, i);
    
    json_object_object_get_ex(poem_elem, "title", &tmp_elem);
    poem->title = json_object_get_string(tmp_elem);
    
    json_object_object_get_ex(poem_elem, "author", &tmp_elem);
    poem->author = json_object_get_string(tmp_elem);
    
    json_object_object_get_ex(poem_elem, "paragraphs", &tmp_elem);
    para_count = json_object_array_length(tmp_elem);
    
    for(int j=0; j<para_count; j++) {
      para_elem = json_object_array_get_idx(tmp_elem, j);
      strcat(buf, json_object_get_string(para_elem));
      if (j != (para_count - 1))
        strcat(buf, "\n");
    }
    poem->paragraphs = buf;
    insert_single_poem(db, poem);
    memset(buf, 0, sizeof(buf));
  }

  // end sqlite transaction
  sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
  
  json_object_put(tmp_elem);
  json_object_put(poem_elem);
  json_object_put(para_elem);
  free(poem);
  sqlite3_close(db);
  return poem_count;
}

int insert_single_poem(sqlite3* db, struct poem* poem) {  
  char* zErrMsg = 0;

  char* sql = build_insert_sql(poem);
  /* printf("sql: %s\n", sql); */
  int rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
  if(rc != SQLITE_OK){
    fprintf(stderr, "sql error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  free(sql);
  
  return 0;
}

char* build_insert_sql(struct poem* poem) {
  // fixme: may encounter segment fault if the memory size is too small
  char* sql = malloc(1024*32);
  sprintf(sql, "INSERT INTO poem VALUES (\"%s\", \"%s\", \"%s\");", poem->title, poem->author, poem->paragraphs);
  return sql;
}