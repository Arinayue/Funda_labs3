#ifndef FUNC_H
#define FUNC_H

#include "task_string.h"
#include "status.h"

typedef struct {
    String city;
    String street;
    int house_number;
    String building;
    int apartment_number;
    String postal_code;
} Address;

typedef struct {
    Address recipient;
    double weight;
    String tracking_id;
    String creation_time;
    String deadline;
    String delivery_time;
} Mail;

typedef struct {
    Address *post_office_address;
    Mail *mails;
    int count;
    int capacity;
} Post;

Status address_init(Address *addr, const char *city, const char *street, int house, const char *building, int apt, const char *postal);
Status address_free(Address *addr);
Status mail_init(Mail *mail, const Address *rec, double weight, const char *id,
                 const char *created, const char *deadline, const char *delivered);
Status mail_free(Mail *mail);

Status post_init(Post *post, const Address *office_addr);
Status post_free(Post *post);
Status post_add_mail(Post *post, const Mail *mail);
Status post_remove_mail(Post *post, const String *tracking_id);
Status post_search_mail(const Post *post, const String *tracking_id, const Mail **out_mail);
Status post_filter(const Post *post, const String *now, int delivered,
                   const Mail ***out_list, int *out_count);

Status validate_postal_code(const String *code);
Status validate_tracking_id(const String *id);
Status validate_datetime(const String *dt);

#endif
