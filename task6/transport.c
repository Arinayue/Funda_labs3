#include "transport.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static Status create_transport(const char *id, size_t id_length,
                                 transport_t **vehicle)
{
    if (id == NULL || id_length == 0 || vehicle == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    if (id_length == SIZE_MAX) {
        return ST_RANGE_ERROR;
    }

    transport_t *new_vehicle = malloc(sizeof(*new_vehicle));
    if (new_vehicle == NULL) {
        return ST_NO_MEMORY;
    }

    char *id_copy = malloc(id_length + 1);
    if (id_copy == NULL) {
        free(new_vehicle);
        return ST_NO_MEMORY;
    }

    memcpy(id_copy, id, id_length);
    id_copy[id_length] = '\0';
    *new_vehicle = (transport_t){id_copy, id_length, NULL, NULL};
    *vehicle = new_vehicle;
    return ST_OK;
}

Status insert_stop(transport_t **head, const char *id, size_t id_length,
                     const stop_data_t *stop)
{
    if (head == NULL || id == NULL || id_length == 0 || stop == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    if (id_length == SIZE_MAX) {
        return ST_RANGE_ERROR;
    }
    const uintmax_t max_timestamp = UINTMAX_C(315537897599);
    if (memchr(id, '\0', id_length) != NULL || !isfinite(stop->point.x) ||
        !isfinite(stop->point.y) || stop->arrival > stop->departure ||
        stop->departure > max_timestamp ||
        (stop->kind != STOP_START && stop->kind != STOP_INTERMEDIATE &&
         stop->kind != STOP_END)) {
        return ST_INVALID_ARGUMENT;
    }

    transport_t **position = head;
    transport_t *vehicle = NULL;
    while (*position != NULL) {
        transport_t *current = *position;
        size_t common_length = current->id_length < id_length
                               ? current->id_length : id_length;
        int order = memcmp(current->id, id, common_length);
        if (order == 0 && current->id_length == id_length) {
            vehicle = current;
            break;
        }
        if (order > 0 || (order == 0 && current->id_length > id_length)) {
            break;
        }
        position = &current->next;
    }

    stop_node_t *new_stop = malloc(sizeof(*new_stop));
    if (new_stop == NULL) {
        return ST_NO_MEMORY;
    }
    new_stop->data = *stop;

    if (vehicle == NULL) {
        Status status = create_transport(id, id_length, &vehicle);
        if (status != ST_OK) {
            free(new_stop);
            return status;
        }
        new_stop->next = NULL;
        vehicle->stops = new_stop;
        vehicle->next = *position;
        *position = vehicle;
        return ST_OK;
    }

    stop_node_t **stop_position = &vehicle->stops;
    while (*stop_position != NULL) {
        const stop_data_t *current = &(*stop_position)->data;
        if (current->arrival > stop->arrival ||
            (current->arrival == stop->arrival && current->departure > stop->departure)) {
            break;
        }
        stop_position = &(*stop_position)->next;
    }
    new_stop->next = *stop_position;
    *stop_position = new_stop;
    return ST_OK;
}

static Status validate_stop_sequence(const stop_node_t *stops)
{
    if (stops == NULL) {
        return ST_INVALID_ROUTE;
    }

    bool route_open = false;
    const stop_data_t *previous = NULL;
    for (const stop_node_t *node = stops; node != NULL; node = node->next) {
        const stop_data_t *current = &node->data;
        if (current->departure < current->arrival ||
            (previous != NULL &&
             (previous->departure > current->arrival ||
              (previous->arrival == current->arrival &&
               previous->departure == current->departure)))) {
            return ST_INVALID_ROUTE;
        }

        switch (current->kind) {
            case STOP_START:
                if (route_open) {
                    return ST_INVALID_ROUTE;
                }
                route_open = true;
                break;
            case STOP_INTERMEDIATE:
            case STOP_END:
                if (!route_open) {
                    return ST_INVALID_ROUTE;
                }
                route_open = current->kind == STOP_INTERMEDIATE;
                break;
            default:
                return ST_INVALID_ROUTE;
        }

        previous = current;
    }

    return route_open ? ST_INVALID_ROUTE : ST_OK;
}

Status validate_routes(const transport_t *head, const char **invalid_id)
{
    if (invalid_id == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    *invalid_id = NULL;

    for (const transport_t *vehicle = head; vehicle != NULL; vehicle = vehicle->next) {
        if (vehicle->id == NULL || vehicle->id[0] == '\0') {
            return ST_INVALID_ARGUMENT;
        }

        Status status = validate_stop_sequence(vehicle->stops);
        if (status != ST_OK) {
            *invalid_id = vehicle->id;
            return status;
        }
    }

    return ST_OK;
}

static Status add_distances(double first, double second, double *total)
{
    if (total == NULL || !isfinite(first) || !isfinite(second) ||
        first < 0.0 || second < 0.0) {
        return ST_INVALID_ARGUMENT;
    }
    if (fmin(first, second) > DBL_MAX - fmax(first, second)) {
        return ST_RANGE_ERROR;
    }

    *total = first + second;
    return ST_OK;
}

static Status coordinate_distance(double first, double second, double *distance)
{
    if (distance == NULL || !isfinite(first) || !isfinite(second)) {
        return ST_INVALID_ARGUMENT;
    }
    if ((signbit(first) == 0) != (signbit(second) == 0)) {
        return add_distances(fabs(first), fabs(second), distance);
    }

    *distance = fabs(first - second);
    return ST_OK;
}

static Status stop_distance(const point_t *first, const point_t *second, double *distance)
{
    if (first == NULL || second == NULL || distance == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    double dx;
    Status status = coordinate_distance(first->x, second->x, &dx);
    if (status != ST_OK) {
        return status;
    }
    double dy;
    status = coordinate_distance(first->y, second->y, &dy);
    if (status != ST_OK) {
        return status;
    }

    double length = hypot(dx, dy);
    if (!isfinite(length) || length == HUGE_VAL) {
        return ST_RANGE_ERROR;
    }

    *distance = length;
    return ST_OK;
}

static Status find_distance(const transport_t *head, metric_t metric, extreme_t extreme,
                              double epsilon, const char **id)
{
    if (head == NULL || id == NULL || !isfinite(epsilon) || epsilon <= 0.0 ||
        (metric != METRIC_TOTAL_DISTANCE && metric != METRIC_ROUTE_DISTANCE) ||
        (extreme != EXTREME_MIN && extreme != EXTREME_MAX)) {
        return ST_INVALID_ARGUMENT;
    }

    const char *best_id = NULL;
    double best_distance = 0.0;
    for (const transport_t *vehicle = head; vehicle != NULL; vehicle = vehicle->next) {
        if (vehicle->id == NULL || vehicle->id[0] == '\0' || vehicle->stops == NULL) {
            return ST_INVALID_ARGUMENT;
        }

        double total_distance = 0.0;
        double route_distance = 0.0;
        const point_t *previous = NULL;
        for (const stop_node_t *node = vehicle->stops; node != NULL; node = node->next) {
            if (node->data.kind == STOP_START) {
                route_distance = 0.0;
            } else {
                double leg;
                Status status = stop_distance(previous, &node->data.point, &leg);
                if (status != ST_OK) {
                    return status;
                }
                status = add_distances(route_distance, leg, &route_distance);
                if (status != ST_OK) {
                    return status;
                }
            }
            previous = &node->data.point;

            if (node->data.kind != STOP_END) {
                continue;
            }

            double candidate = route_distance;
            if (metric == METRIC_TOTAL_DISTANCE) {
                Status status = add_distances(total_distance, route_distance, &total_distance);
                if (status != ST_OK) {
                    return status;
                }
                if (node->next != NULL) {
                    continue;
                }
                candidate = total_distance;
            }

            double improvement = extreme == EXTREME_MIN ? best_distance - candidate
                                                        : candidate - best_distance;
            if (best_id == NULL || improvement > epsilon) {
                best_id = vehicle->id;
                best_distance = candidate;
            }
        }
    }

    if (best_id == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    *id = best_id;
    return ST_OK;
}

static Status measure_count_or_time(const transport_t *vehicle, metric_t metric,
                                      extreme_t extreme, uintmax_t *value)
{
    if (vehicle == NULL || vehicle->stops == NULL || value == NULL ||
        (metric != METRIC_ROUTE_COUNT && metric != METRIC_STOP_DURATION &&
         metric != METRIC_TOTAL_STOP_DURATION) ||
        (extreme != EXTREME_MIN && extreme != EXTREME_MAX) ||
        (metric == METRIC_TOTAL_STOP_DURATION && extreme != EXTREME_MAX)) {
        return ST_INVALID_ARGUMENT;
    }

    uintmax_t result = 0;
    if (metric == METRIC_STOP_DURATION && extreme == EXTREME_MIN) {
        result = UINTMAX_MAX;
    }

    for (const stop_node_t *node = vehicle->stops; node != NULL; node = node->next) {
        uintmax_t sample;
        if (metric == METRIC_ROUTE_COUNT) {
            if (node->data.kind != STOP_END) {
                continue;
            }
            sample = 1;
        } else {
            if (node->data.departure < node->data.arrival) {
                return ST_INVALID_ARGUMENT;
            }
            sample = node->data.departure - node->data.arrival;
        }

        if (metric == METRIC_STOP_DURATION) {
            if (extreme == EXTREME_MIN ? sample < result : sample > result) {
                result = sample;
            }
        } else {
            if (sample > UINTMAX_MAX - result) {
                return ST_RANGE_ERROR;
            }
            result += sample;
        }
    }

    *value = result;
    return ST_OK;
}

Status find_vehicle(const transport_t *head, metric_t metric, extreme_t extreme,
                      double epsilon, const char **id)
{
    if (id == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    *id = NULL;

    if (!isfinite(epsilon) || epsilon <= 0.0 ||
        (extreme != EXTREME_MIN && extreme != EXTREME_MAX) ||
        (metric == METRIC_TOTAL_STOP_DURATION && extreme != EXTREME_MAX) ||
        (metric != METRIC_ROUTE_COUNT && metric != METRIC_TOTAL_DISTANCE &&
         metric != METRIC_ROUTE_DISTANCE && metric != METRIC_STOP_DURATION &&
         metric != METRIC_TOTAL_STOP_DURATION)) {
        return ST_INVALID_ARGUMENT;
    }
    if (head == NULL) {
        return ST_NOT_FOUND;
    }
    if (metric == METRIC_TOTAL_DISTANCE || metric == METRIC_ROUTE_DISTANCE) {
        return find_distance(head, metric, extreme, epsilon, id);
    }

    const char *best_id = NULL;
    uintmax_t best_value = 0;
    for (const transport_t *vehicle = head; vehicle != NULL; vehicle = vehicle->next) {
        if (vehicle->id == NULL || vehicle->id[0] == '\0') {
            return ST_INVALID_ARGUMENT;
        }

        uintmax_t value;
        Status status = measure_count_or_time(vehicle, metric, extreme, &value);
        if (status != ST_OK) {
            return status;
        }
        if (best_id == NULL || (extreme == EXTREME_MIN ? value < best_value : value > best_value)) {
            best_id = vehicle->id;
            best_value = value;
        }
    }

    *id = best_id;
    return ST_OK;
}

Status clear_transport(transport_t **head)
{
    if (head == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    while (*head != NULL) {
        transport_t *vehicle = *head;
        *head = vehicle->next;

        stop_node_t *stop = vehicle->stops;
        while (stop != NULL) {
            stop_node_t *next = stop->next;
            free(stop);
            stop = next;
        }

        free(vehicle->id);
        free(vehicle);
    }

    return ST_OK;
}
