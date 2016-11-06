#include "comm.h"

static void in_recv_handler(DictionaryIterator *iter, void *context) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "recv handler start");
    
  Tuple *t = dict_read_first(iter);
  while(t) {
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "key %i, str %s, bool %i", (int)t->key, t->value->cstring, (int)t->value->int32);
      
      if (t->key == PERSIST_KEY_LIGHT_THEME)
          persist_write_bool(t->key, strcmp(t->value->cstring, "light") == 0 ? true : false);
      else if (t->key == PERSIST_KEY_DATE_STYLE) {
          bool date = false;
          bool day = false;
          if (strcmp(t->value->cstring, "date") == 0)
              date = true;
          else if (strcmp(t->value->cstring, "all") == 0)
              day = date = true;
          
          persist_write_bool(PERSIST_KEY_DATE, date);
          persist_write_bool(PERSIST_KEY_DAY, day);
      }
      else //if (!(t->key == PERSIST_KEY_DATE || t->key == PERSIST_KEY_DAY)) // HACKY :/
          persist_write_bool(t->key, t->value->int32);
    t = dict_read_next(iter);
  }

  // Refresh live store
  config_init();
  vibes_short_pulse();

  window_stack_pop_all(true);
}

void comm_init() {
  app_message_register_inbox_received(in_recv_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}