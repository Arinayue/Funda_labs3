#ifndef TASK6_TRANSPORT_H
#define TASK6_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

typedef struct {
    double x;
    double y;
} point_t;

typedef enum {
    STOP_START,
    STOP_INTERMEDIATE,
    STOP_END
} stop_kind_t;

typedef struct {
    point_t point;
    uintmax_t arrival;
    uintmax_t departure;
    stop_kind_t kind;
} stop_data_t;

typedef struct stop_node {
    stop_data_t data;
    struct stop_node *next;
} stop_node_t;

typedef struct transport {
    char *id;
    size_t id_length;
    stop_node_t *stops;
    struct transport *next;
} transport_t;

typedef enum {
    METRIC_ROUTE_COUNT,
    METRIC_TOTAL_DISTANCE,
    METRIC_ROUTE_DISTANCE,
    METRIC_STOP_DURATION,
    METRIC_TOTAL_STOP_DURATION
} metric_t;

typedef enum {
    EXTREME_MIN,
    EXTREME_MAX
} extreme_t;

Status insert_stop(transport_t **head, const char *id, size_t id_length,
                     const stop_data_t *stop);
Status validate_routes(const transport_t *head, const char **invalid_id);
Status find_vehicle(const transport_t *head, metric_t metric, extreme_t extreme,
                      double epsilon, const char **id);
Status clear_transport(transport_t **head);

#endif
