extern int g_connection;
extern struct display_manager g_display_manager;
extern struct space_manager g_space_manager;
extern struct window_manager g_window_manager;

#define INSERT_FEEDBACK_WIDTH 2
#define INSERT_FEEDBACK_RADIUS 9
void insert_feedback_show(struct window_node *node)
{
    CFTypeRef frame_region;
    CGRect frame = {{node->area.x, node->area.y},{node->area.w, node->area.h}};
    CGSNewRegionWithRect(&frame, &frame_region);
    frame.origin.x = 0; frame.origin.y = 0;

    if (!node->feedback_window.id) {
        uint64_t tags = (1ULL << 1) | (1ULL << 9);
        CFTypeRef empty_region = CGRegionCreateEmptyRegion();
        SLSNewWindowWithOpaqueShapeAndContext(g_connection, 2, frame_region, empty_region, 13, &tags, 0, 0, 64, &node->feedback_window.id, NULL);
        CFRelease(empty_region);

        sls_window_disable_shadow(node->feedback_window.id);
        SLSSetWindowResolution(g_connection, node->feedback_window.id, 1.0f);
        SLSSetWindowOpacity(g_connection, node->feedback_window.id, 0);
        SLSSetWindowLevel(g_connection, node->feedback_window.id, window_level(node->window_order[0]));
        SLSSetWindowSubLevel(g_connection, node->feedback_window.id, window_sub_level(node->window_order[0]));
        node->feedback_window.context = SLWindowContextCreate(g_connection, node->feedback_window.id, 0);
        CGContextSetLineWidth(node->feedback_window.context, INSERT_FEEDBACK_WIDTH);
        CGContextSetRGBFillColor(node->feedback_window.context,
                                   g_window_manager.insert_feedback_color.r,
                                   g_window_manager.insert_feedback_color.g,
                                   g_window_manager.insert_feedback_color.b,
                                   g_window_manager.insert_feedback_color.a*0.25f);
        CGContextSetRGBStrokeColor(node->feedback_window.context,
                                   g_window_manager.insert_feedback_color.r,
                                   g_window_manager.insert_feedback_color.g,
                                   g_window_manager.insert_feedback_color.b,
                                   g_window_manager.insert_feedback_color.a);
        SLSDisableUpdate(g_connection);
        CGContextClearRect(node->feedback_window.context, frame);
        CGContextFlush(node->feedback_window.context);
        SLSReenableUpdate(g_connection);
        SLSOrderWindow(g_connection, node->feedback_window.id, 1, node->window_order[0]);
        table_add(&g_window_manager.insert_feedback, &node->window_order[0], node);
        if (!workspace_is_macos_sequoia() && !workspace_is_macos_tahoe()) {
            update_window_notifications();
        }
    }

    CGFloat clip_x, clip_y, clip_w, clip_h;
    CGFloat midx = CGRectGetMidX(frame);
    CGFloat midy = CGRectGetMidY(frame);

    switch (node->insert_dir) {
    case DIR_NORTH: {
        clip_x = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_y = midy - 0.5f * INSERT_FEEDBACK_WIDTH;
        clip_w = INSERT_FEEDBACK_WIDTH;
        clip_h = INSERT_FEEDBACK_WIDTH;
    } break;
    case DIR_EAST: {
        clip_x = midx - 0.5f * INSERT_FEEDBACK_WIDTH;
        clip_y = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_w = INSERT_FEEDBACK_WIDTH;
        clip_h = INSERT_FEEDBACK_WIDTH;
    } break;
    case DIR_SOUTH: {
        clip_x = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_y = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_w = INSERT_FEEDBACK_WIDTH;
        clip_h = -midy + INSERT_FEEDBACK_WIDTH;
    } break;
    case DIR_WEST: {
        clip_x = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_y = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_w = -midx + INSERT_FEEDBACK_WIDTH;
        clip_h = INSERT_FEEDBACK_WIDTH;
    } break;
    case STACK: {
        clip_x = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_y = -0.5f * INSERT_FEEDBACK_WIDTH;
        clip_w = INSERT_FEEDBACK_WIDTH;
        clip_h = INSERT_FEEDBACK_WIDTH;
    } break;
    }

    CGRect rect = (CGRect) {{ 0.5f*INSERT_FEEDBACK_WIDTH, 0.5f*INSERT_FEEDBACK_WIDTH }, { frame.size.width - INSERT_FEEDBACK_WIDTH, frame.size.height - INSERT_FEEDBACK_WIDTH }};
    CGRect fill = CGRectInset(rect, 0.5f*INSERT_FEEDBACK_WIDTH, 0.5f*INSERT_FEEDBACK_WIDTH);
    CGRect clip = { { rect.origin.x + clip_x, rect.origin.y + clip_y }, { rect.size.width + clip_w, rect.size.height + clip_h } };
    CGPathRef path = CGPathCreateWithRoundedRect(rect, cgrect_clamp_x_radius(rect, INSERT_FEEDBACK_RADIUS), cgrect_clamp_y_radius(rect, INSERT_FEEDBACK_RADIUS), NULL);

    SLSDisableUpdate(g_connection);
    SLSSetWindowShape(g_connection, node->feedback_window.id, 0.0f, 0.0f, frame_region);
    CGContextClearRect(node->feedback_window.context, frame);
    CGContextClipToRect(node->feedback_window.context, clip);
    CGContextFillRect(node->feedback_window.context, fill);
    CGContextAddPath(node->feedback_window.context, path);
    CGContextStrokePath(node->feedback_window.context);
    CGContextResetClip(node->feedback_window.context);
    CGContextFlush(node->feedback_window.context);
    SLSReenableUpdate(g_connection);
    CGPathRelease(path);
    CFRelease(frame_region);
}

void insert_feedback_destroy(struct window_node *node)
{
    if (node->feedback_window.id) {
        table_remove(&g_window_manager.insert_feedback, &node->window_order[0]);

        if (!workspace_is_macos_sequoia() && !workspace_is_macos_tahoe()) {
            update_window_notifications();
        }

        SLSOrderWindow(g_connection, node->feedback_window.id, 0, 0);
        CGContextRelease(node->feedback_window.context);
        SLSReleaseWindow(g_connection, node->feedback_window.id);
        memset(&node->feedback_window, 0, sizeof(struct feedback_window));
    }
}

static inline struct area area_from_cgrect(CGRect rect)
{
    return (struct area) { rect.origin.x, rect.origin.y, rect.size.width, rect.size.height };
}

static inline CGPoint area_max_point(struct area area)
{
    return (CGPoint) { area.x + area.w - 1, area.y + area.h - 1 };
}

static inline enum window_node_child window_node_get_child(struct window_node *node)
{
    return node->child != CHILD_NONE ? node->child : g_space_manager.window_placement;
}

static inline enum window_node_split window_node_get_split(struct view *view, struct window_node *node)
{
    if (node->split != SPLIT_NONE) return node->split;

    if (view->split_type != SPLIT_NONE) {
        if (view->split_type != SPLIT_AUTO) {
            return view->split_type;
        }
    } else if (g_space_manager.split_type != SPLIT_AUTO) {
        return g_space_manager.split_type;
    }

    return node->area.w >= node->area.h ? SPLIT_Y : SPLIT_X;
}

static inline float window_node_get_ratio(struct window_node *node)
{
    return in_range_ii(node->ratio, 0.1f, 0.9f) ? node->ratio : g_space_manager.split_ratio;
}

static inline int window_node_get_gap(struct view *view)
{
    return view_check_flag(view, VIEW_ENABLE_GAP) ? view->window_gap : 0;
}

static void area_make_pair(enum window_node_split split, int gap, float ratio, struct area *parent_area, struct area *left_area, struct area *right_area)
{
    if (split == SPLIT_Y) {
        *left_area  = *parent_area;
        *right_area = *parent_area;

        float left_width  = (parent_area->w - gap) * ratio;
        float right_width = (parent_area->w - gap) * (1 - ratio);

        left_area->w   = (int)left_width;
        right_area->w  = (int)right_width;
        right_area->x += (int)(left_width + 0.5f) + gap;
    } else {
        *left_area  = *parent_area;
        *right_area = *parent_area;

        float left_width  = (parent_area->h - gap) * ratio;
        float right_width = (parent_area->h - gap) * (1 - ratio);

        left_area->h   = (int)left_width;
        right_area->h  = (int)right_width;
        right_area->y += (int)(left_width + 0.5f) + gap;
    }
}

static void calculate_two_column_areas(struct view *view, struct area *parent, struct area *main, struct area *stack_right, int stack_count)
{
    int gap = window_node_get_gap(view);
    float ratio = view->main_ratio;

    printf("[AREA] calculate_two_column: ratio=%.3f, flag=0x%llx, stack_count=%d\n", ratio, view->flags, stack_count);

    *main = *parent;
    *stack_right = *parent;

    // If no stack windows, main takes 100%
    if (stack_count == 0) {
        main->w = parent->w;
        stack_right->w = 0;
        return;
    }

    // Normal case: split according to ratio
    float main_width = (parent->w - gap) * ratio;
    float stack_width = (parent->w - gap) * (1 - ratio);

    main->w = (int)main_width;
    stack_right->w = (int)stack_width;
    stack_right->x = parent->x + (int)(main_width + 0.5f) + gap;
}

static void calculate_three_column_areas(struct view *view, struct area *parent, struct area *stack_left, struct area *main, struct area *stack_right, int stack_count)
{
    int gap = window_node_get_gap(view);
    float main_ratio = view->main_ratio;

    *stack_left = *parent;
    *main = *parent;
    *stack_right = *parent;

    // If no stack windows at all, main takes 100%
    if (stack_count == 0) {
        main->w = parent->w;
        stack_left->w = 0;
        stack_right->w = 0;
        return;
    }

    // If only one stack window, behave like two-column (no left stack)
    if (stack_count == 1) {
        float main_width = (parent->w - gap) * main_ratio;
        float stack_width = (parent->w - gap) * (1.0f - main_ratio);

        stack_left->w = 0;
        main->w = (int)main_width;
        stack_right->w = (int)stack_width;
        stack_right->x = parent->x + (int)(main_width + 0.5f) + gap;
        return;
    }

    // Normal case: split stack between left and right
    float stack_ratio = (1.0f - main_ratio) / 2.0f;
    int total_gap = gap * 2;
    float stack_left_width = (parent->w - total_gap) * stack_ratio;
    float main_width = (parent->w - total_gap) * main_ratio;
    float stack_right_width = (parent->w - total_gap) * stack_ratio;

    stack_left->w = (int)stack_left_width;
    main->w = (int)main_width;
    stack_right->w = (int)stack_right_width;

    main->x = parent->x + (int)(stack_left_width + 0.5f) + gap;
    stack_right->x = main->x + main->w + gap;
}

static void area_make_main_stack(struct view *view, struct area *parent, struct area *main, struct area *stack_left, struct area *stack_right, int stack_count)
{
    switch (view->main_variant) {
        case MAIN_VARIANT_TWO_COLUMN:
            calculate_two_column_areas(view, parent, main, stack_right, stack_count);
            break;
        case MAIN_VARIANT_THREE_COLUMN:
            calculate_three_column_areas(view, parent, stack_left, main, stack_right, stack_count);
            break;
    }
}

static void assign_regions_to_windows(struct view *view, struct window_node *node)
{
    int nmain = view->main_nmain;
    int main_end = (node->window_count < nmain) ? node->window_count : nmain;

    // Assign regions: main (0..nmain-1) and stack (nmain..count-1)
    for (int i = 0; i < node->window_count; i++) {
        if (i < main_end) {
            node->window_regions[i] = REGION_MAIN;
        } else {
            node->window_regions[i] = REGION_STACK;
        }
    }
}


static void tile_windows_in_region(struct area region_area, uint32_t *window_ids, float *window_ratios, int count, int gap, struct window_capture **captures)
{
    if (count == 0) {
        return;
    }
    debug("tile_windows_in_region: count=%d, area=(%.0f,%.0f,%.0fx%.0f)\n",
          count, region_area.x, region_area.y, region_area.w, region_area.h);

    if (count == 1) {
        struct window *w = window_manager_find_window(&g_window_manager, window_ids[0]);
        if (w) {
            debug("  -> window %d at (%.0f,%.0f,%.0fx%.0f)\n",
                  window_ids[0], region_area.x, region_area.y, region_area.w, region_area.h);
            ts_buf_push(*captures, ((struct window_capture) {
                .window = w,
                .x = region_area.x,
                .y = region_area.y,
                .w = region_area.w,
                .h = region_area.h
            }));
        }
        return;
    }

    struct area top_area = region_area;
    struct area bottom_area = region_area;

    // Calculate sum of ratios to renormalize (ratios must sum to 1.0 for current window set)
    float sum_of_ratios = 0;
    for (int i = 0; i < count; i++) {
        sum_of_ratios += window_ratios[i];
    }

    // Split ratio is this window's ratio relative to all current windows
    float split_ratio = (sum_of_ratios > 0) ? (window_ratios[0] / sum_of_ratios) : 0.5f;
    float top_height = (region_area.h - gap) * split_ratio;


    top_area.h = (int)top_height;
    bottom_area.h = region_area.h - (int)top_height - gap;
    bottom_area.y = region_area.y + (int)top_height + gap;


    struct window *w = window_manager_find_window(&g_window_manager, window_ids[0]);
    if (w) {
        debug("  -> window %d at (%.0f,%.0f,%.0fx%.0f)\n",
              window_ids[0], top_area.x, top_area.y, top_area.w, top_area.h);
        ts_buf_push(*captures, ((struct window_capture) {
            .window = w,
            .x = top_area.x,
            .y = top_area.y,
            .w = top_area.w,
            .h = top_area.h
        }));
    }

    tile_windows_in_region(bottom_area, window_ids + 1, window_ratios + 1, count - 1, gap, captures);
}

static void area_make_pair_for_node(struct view *view, struct window_node *node)
{
    enum window_node_split split = window_node_get_split(view, node);
    float ratio = window_node_get_ratio(node);
    int gap     = window_node_get_gap(view);

    area_make_pair(split, gap, ratio, &node->area, &node->left->area, &node->right->area);

    node->split = split;
    node->ratio = ratio;
}

static inline bool window_node_is_occupied(struct window_node *node)
{
    return node->window_count != 0;
}

static inline bool window_node_is_intermediate(struct window_node *node)
{
    return node->parent != NULL;
}

static inline bool window_node_is_leaf(struct window_node *node)
{
    return node->left == NULL && node->right == NULL;
}

static inline bool window_node_is_left_child(struct window_node *node)
{
    return node->parent && node->parent->left == node;
}

static inline bool window_node_is_right_child(struct window_node *node)
{
    return node->parent && node->parent->right == node;
}

static void window_node_equalize(struct window_node *node, uint32_t axis_flag)
{
    if (node->left)  window_node_equalize(node->left, axis_flag);
    if (node->right) window_node_equalize(node->right, axis_flag);

    if ((axis_flag & SPLIT_Y) && node->split == SPLIT_Y) {
        node->ratio = g_space_manager.split_ratio;
    }

    if ((axis_flag & SPLIT_X) && node->split == SPLIT_X) {
        node->ratio = g_space_manager.split_ratio;
    }
}

static inline struct balance_node balance_node_add(struct balance_node a, struct balance_node b)
{
    return (struct balance_node) { a.y_count + b.y_count, a.x_count + b.x_count, };
}

static struct balance_node window_node_balance(struct window_node *node, uint32_t axis_flag)
{
    if (window_node_is_leaf(node)) {
        return (struct balance_node) {
            node->parent ? node->parent->split == SPLIT_Y : 0,
            node->parent ? node->parent->split == SPLIT_X : 0
        };
    }

    struct balance_node left_leafs  = window_node_balance(node->left, axis_flag);
    struct balance_node right_leafs = window_node_balance(node->right, axis_flag);
    struct balance_node total_leafs = balance_node_add(left_leafs, right_leafs);

    if (axis_flag & SPLIT_Y) {
        if (node->split == SPLIT_Y) {
            node->ratio = (float) left_leafs.y_count / total_leafs.y_count;
            --total_leafs.y_count;
        }
    }

    if (axis_flag & SPLIT_X) {
        if (node->split == SPLIT_X) {
            node->ratio = (float) left_leafs.x_count / total_leafs.x_count;
            --total_leafs.x_count;
        }
    }

    if (node->parent) {
        total_leafs.y_count += node->parent->split == SPLIT_Y;
        total_leafs.x_count += node->parent->split == SPLIT_X;
    }

    return total_leafs;
}

static void window_node_split(struct view *view, struct window_node *node, struct window *window)
{
    struct window_node *left = malloc(sizeof(struct window_node));
    memset(left, 0, sizeof(struct window_node));

    struct window_node *right = malloc(sizeof(struct window_node));
    memset(right, 0, sizeof(struct window_node));

    struct window_node *zoom = !g_space_manager.window_zoom_persist
                             ? NULL
                             : !node->zoom
                             ? NULL
                             : node->zoom == node->parent
                             ? node
                             : view->root;

    if (window_node_get_child(node) == CHILD_SECOND) {
        memcpy(left->window_list, node->window_list, sizeof(uint32_t) * node->window_count);
        memcpy(left->window_order, node->window_order, sizeof(uint32_t) * node->window_count);
        left->window_count = node->window_count;
        left->zoom = zoom;

        right->window_list[0] = window->id;
        right->window_order[0] = window->id;
        right->window_count = 1;
    } else {
        memcpy(right->window_list, node->window_list, sizeof(uint32_t) * node->window_count);
        memcpy(right->window_order, node->window_order, sizeof(uint32_t) * node->window_count);
        right->window_count = node->window_count;
        right->zoom = zoom;

        left->window_list[0] = window->id;
        left->window_order[0] = window->id;
        left->window_count = 1;
    }

    left->parent  = node;
    right->parent = node;

    node->window_count = 0;
    node->left  = left;
    node->right = right;
    node->zoom  = NULL;

    area_make_pair_for_node(view, node);
}

void window_node_update(struct view *view, struct window_node *node)
{
    if (window_node_is_leaf(node)) {
        if (node->insert_dir) insert_feedback_show(node);
    } else {
        area_make_pair_for_node(view, node);
        window_node_update(view, node->left);
        window_node_update(view, node->right);
    }
}

static void window_node_destroy(struct window_node *node)
{
    if (node->left)  window_node_destroy(node->left);
    if (node->right) window_node_destroy(node->right);

    for (int i = 0; i < node->window_count; ++i) {
        window_manager_remove_managed_window(&g_window_manager, node->window_list[i]);
    }

    insert_feedback_destroy(node);
    free(node);
}

static void window_node_clear_zoom(struct window_node *node)
{
    node->zoom = NULL;

    if (!window_node_is_leaf(node)) {
        window_node_clear_zoom(node->left);
        window_node_clear_zoom(node->right);
    }
}

void window_node_capture_windows(struct window_node *node, struct window_capture **window_list)
{
    if (window_node_is_leaf(node)) {
        // Check if this node has region assignments (main-stack layout)
        // If so, skip capturing - view_flush_main_stack already handled it
        bool has_regions = false;
        for (int i = 0; i < node->window_count; ++i) {
            if (node->window_regions[i] != 0) {
                has_regions = true;
                break;
            }
        }

        if (has_regions) {
            debug("window_node_capture_windows: Skipping main-stack node with %d windows\n", node->window_count);
            return;
        }

        for (int i = 0; i < node->window_count; ++i) {
            struct window *window = window_manager_find_window(&g_window_manager, node->window_list[i]);
            if (window) {
                struct area area = node->zoom ? node->zoom->area : node->area;
                ts_buf_push(*window_list, ((struct window_capture) { .window = window, .x = area.x, .y = area.y, .w = area.w, .h = area.h }));
            }
        }
    } else {
        window_node_capture_windows(node->left, window_list);
        window_node_capture_windows(node->right, window_list);
    }
}

void window_node_flush(struct window_node *node)
{
    debug("window_node_flush called! node=%p, window_count=%d\n", (void*)node, node ? node->window_count : -1);
    struct window_capture *window_list = NULL;
    window_node_capture_windows(node, &window_list);
    if (window_list) {
        int count = ts_buf_len(window_list);
        debug("window_node_flush animating %d windows\n", count);
        window_manager_animate_window_list(window_list, count);
    }
}

bool window_node_contains_window(struct window_node *node, uint32_t window_id)
{
    for (int i = 0; i < node->window_count; ++i) {
        if (node->window_list[i] == window_id) return true;
    }

    return false;
}

int window_node_index_of_window(struct window_node *node, uint32_t window_id)
{
    for (int i = 0; i < node->window_count; ++i) {
        if (node->window_list[i] == window_id) return i;
    }

    return 0;
}

void window_node_swap_window_list(struct window_node *a_node, struct window_node *b_node)
{
    uint32_t tmp_window_list[NODE_MAX_WINDOW_COUNT];
    uint32_t tmp_window_order[NODE_MAX_WINDOW_COUNT];
    uint32_t tmp_window_count;

    memcpy(tmp_window_list, a_node->window_list, sizeof(uint32_t) * a_node->window_count);
    memcpy(tmp_window_order, a_node->window_order, sizeof(uint32_t) * a_node->window_count);
    tmp_window_count = a_node->window_count;

    memcpy(a_node->window_list, b_node->window_list, sizeof(uint32_t) * b_node->window_count);
    memcpy(a_node->window_order, b_node->window_order, sizeof(uint32_t) * b_node->window_count);
    a_node->window_count = b_node->window_count;

    memcpy(b_node->window_list, tmp_window_list, sizeof(uint32_t) * tmp_window_count);
    memcpy(b_node->window_order, tmp_window_order, sizeof(uint32_t) * tmp_window_count);
    b_node->window_count = tmp_window_count;

    a_node->zoom = NULL;
    b_node->zoom = NULL;
}

struct window_node *window_node_find_first_leaf(struct window_node *root)
{
    struct window_node *node = root;
    while (!window_node_is_leaf(node)) {
        node = node->left;
    }
    return node;
}

struct window_node *window_node_find_last_leaf(struct window_node *root)
{
    struct window_node *node = root;
    while (!window_node_is_leaf(node)) {
        node = node->right;
    }
    return node;
}

struct window_node *window_node_find_prev_leaf(struct window_node *node)
{
    if (!node->parent) return NULL;

    if (window_node_is_left_child(node)) {
        return window_node_find_prev_leaf(node->parent);
    }

    if (window_node_is_leaf(node->parent->left)) {
        return node->parent->left;
    }

    return window_node_find_last_leaf(node->parent->left->right);
}

struct window_node *window_node_find_next_leaf(struct window_node *node)
{
    if (!node->parent) return NULL;

    if (window_node_is_right_child(node)) {
        return window_node_find_next_leaf(node->parent);
    }

    if (window_node_is_leaf(node->parent->right)) {
        return node->parent->right;
    }

    return window_node_find_first_leaf(node->parent->right->left);
}

void window_node_rotate(struct window_node *node, int degrees)
{
    if ((degrees ==  90 && node->split == SPLIT_Y) ||
        (degrees == 270 && node->split == SPLIT_X) ||
        (degrees == 180)) {
        struct window_node *temp = node->left;
        node->left  = node->right;
        node->right = temp;
        node->ratio = 1 - node->ratio;
    }

    if (degrees != 180) {
        if (node->split == SPLIT_X) {
            node->split = SPLIT_Y;
        } else if (node->split == SPLIT_Y) {
            node->split = SPLIT_X;
        }
    }

    if (!window_node_is_leaf(node)) {
        window_node_rotate(node->left, degrees);
        window_node_rotate(node->right, degrees);
    }
}

struct window_node *window_node_mirror(struct window_node *node, enum window_node_split axis)
{
    if (!window_node_is_leaf(node)) {
        struct window_node *left = window_node_mirror(node->left, axis);
        struct window_node *right = window_node_mirror(node->right, axis);

        if (node->split == axis) {
            node->left = right;
            node->right = left;
        }
    }

    return node;
}

struct window_node *window_node_fence(struct window_node *node, int dir)
{
    if (!node) return NULL;

    for (struct window_node *parent = node->parent; parent; parent = parent->parent) {
        if ((dir == DIR_NORTH && parent->split == SPLIT_X && parent->area.y < node->area.y) ||
            (dir == DIR_WEST  && parent->split == SPLIT_Y && parent->area.x < node->area.x) ||
            (dir == DIR_SOUTH && parent->split == SPLIT_X && (parent->area.y + parent->area.h) > (node->area.y + node->area.h)) ||
            (dir == DIR_EAST  && parent->split == SPLIT_Y && (parent->area.x + parent->area.w) > (node->area.x + node->area.w))) {
                return parent;
        }
    }

    return NULL;
}

struct window_node *view_find_min_depth_leaf_node(struct window_node *node)
{
    struct window_node *list[256] = { node };

    for (int i = 0, j = 0; i < 256; ++i) {
        if (window_node_is_leaf(list[i])) {
            return list[i];
        }

        list[++j] = list[i]->left;
        list[++j] = list[i]->right;
    }

    return NULL;
}

static inline bool area_is_in_direction(struct area *r1, CGPoint r1_max, struct area *r2, CGPoint r2_max, int direction)
{
    if (direction == DIR_NORTH && r1_max.y <= r2->y) return false;
    if (direction == DIR_EAST  && r2_max.x <= r1->x) return false;
    if (direction == DIR_SOUTH && r2_max.y <= r1->y) return false;
    if (direction == DIR_WEST  && r1_max.x <= r2->x) return false;

    if (direction == DIR_NORTH || direction == DIR_SOUTH) {
        return ((r2_max.x >  r1->x && r2_max.x <= r1_max.x) ||
                (r2->x    <  r1->x && r2_max.x >  r1_max.x) ||
                (r2->x    >= r1->x && r2->x    <  r1_max.x));
    }

    if (direction == DIR_EAST || direction == DIR_WEST) {
        return ((r2_max.y >  r1->y && r2_max.y <= r1_max.y) ||
                (r2->y    <  r1->y && r2_max.y >  r1_max.y) ||
                (r2->y    >= r1->y && r2->y    <  r1_max.y));
    }

    return false;
}

static inline int area_distance_in_direction(struct area *r1, CGPoint r1_max, struct area *r2, CGPoint r2_max, int direction)
{
    switch (direction) {
    case DIR_NORTH: {
        return r2_max.y > r1->y ? r2_max.y - r1->y : r1->y - r2_max.y;
    } break;
    case DIR_EAST: {
        return r2->x < r1_max.x ? r1_max.x - r2->x : r2->x - r1_max.x;
    } break;
    case DIR_SOUTH: {
        return r2->y < r1_max.y ? r1_max.y - r2->y : r2->y - r1_max.y;
    } break;
    case DIR_WEST: {
        return r2_max.x > r1->x ? r2_max.x - r1->x : r1->x - r2_max.x;
    } break;
    }

    return INT_MAX;
}

struct window_node *view_find_window_node_in_direction(struct view *view, struct window_node *source, int direction)
{
    int window_count;
    uint32_t *window_list = space_window_list(view->sid, &window_count, false);
    if (!window_list) return NULL;

    int best_distance = INT_MAX;
    int best_rank = INT_MAX;
    struct window_node *best_node = NULL;
    CGPoint source_area_max = area_max_point(source->area);

    for (struct window_node *target = window_node_find_first_leaf(view->root); target; target = window_node_find_next_leaf(target)) {
        if (source == target) continue;

        CGPoint target_area_max = area_max_point(target->area);
        if (area_is_in_direction(&source->area, source_area_max, &target->area, target_area_max, direction)) {
            int distance = area_distance_in_direction(&source->area, source_area_max, &target->area, target_area_max, direction);
            int rank = window_manager_find_rank_of_window_in_list(target->window_order[0], window_list, window_count);
            if ((distance < best_distance) || (distance == best_distance && rank < best_rank)) {
                best_node = target;
                best_distance = distance;
                best_rank = rank;
            }
        }
    }

    return best_node;
}

struct window_node *view_find_window_node(struct view *view, uint32_t window_id)
{
    for (struct window_node *node = window_node_find_first_leaf(view->root); node; node = window_node_find_next_leaf(node)) {
        if (window_node_contains_window(node, window_id)) return node;
    }

    return NULL;
}

struct window_node *view_remove_window_node(struct view *view, struct window *window)
{
    struct window_node *node = view_find_window_node(view, window->id);
    if (!node) return NULL;

    if (node->window_count > 1) {
        // Find the indices to remove from both lists
        int entry_index = -1;
        int order_index = -1;

        for (int i = 0; i < node->window_count; ++i) {
            if (node->window_list[i] == window->id) entry_index = i;
            if (node->window_order[i] == window->id) order_index = i;
        }

        assert(entry_index != -1);
        assert(order_index != -1);

        // Shift all arrays at the entry_index (removes from window_list position)
        memmove(node->window_list + entry_index, node->window_list + entry_index + 1,
                sizeof(uint32_t) * (node->window_count - entry_index - 1));
        memmove(node->window_regions + entry_index, node->window_regions + entry_index + 1,
                sizeof(uint8_t) * (node->window_count - entry_index - 1));
        memmove(node->window_ratios + entry_index, node->window_ratios + entry_index + 1,
                sizeof(float) * (node->window_count - entry_index - 1));

        // Shift window_order at the order_index
        memmove(node->window_order + order_index, node->window_order + order_index + 1,
                sizeof(uint32_t) * (node->window_count - order_index - 1));

        --node->window_count;

        if (view->insertion_point == window->id) {
            view->insertion_point = node->window_order[0];
        }

        // Re-tile after removing window (important for main-stack layout)
        view_update(view);
        view_flush(view);

        return NULL;
    }

    if (node == view->root) {
        view->insertion_point = 0;
        insert_feedback_destroy(node);
        memset(node, 0, sizeof(struct window_node));
        view_update(view);
        return NULL;
    }

    struct window_node *parent = node->parent;
    struct window_node *child  = window_node_is_right_child(node)
                               ? parent->left
                               : parent->right;


    memcpy(parent->window_list, child->window_list, sizeof(uint32_t) * child->window_count);
    memcpy(parent->window_order, child->window_order, sizeof(uint32_t) * child->window_count);
    parent->window_count = child->window_count;

    parent->left      = NULL;
    parent->right     = NULL;
    parent->zoom      = !g_space_manager.window_zoom_persist
                      ? NULL
                      : !child->zoom
                      ? NULL
                      : child->zoom == parent
                      ? parent->parent
                      : view->root;

    if (child->insert_dir) {
        parent->feedback_window = child->feedback_window;
        parent->insert_dir      = child->insert_dir;
        parent->split           = child->split;
        parent->child           = child->child;
        insert_feedback_show(parent);
    }

    if (window_node_is_intermediate(child) && !window_node_is_leaf(child)) {
        parent->left          = child->left;
        parent->left->parent  = parent;
        parent->left->zoom    = !g_space_manager.window_zoom_persist
                              ? NULL
                              : !child->left->zoom
                              ? NULL
                              : child->left->zoom == child
                              ? parent
                              : view->root;

        parent->right         = child->right;
        parent->right->parent = parent;
        parent->right->zoom   = !g_space_manager.window_zoom_persist
                              ? NULL
                              : !child->right->zoom
                              ? NULL
                              : child->right->zoom == child
                              ? parent
                              : view->root;

        if (!g_space_manager.window_zoom_persist) {
            window_node_clear_zoom(parent);
        }

        window_node_update(view, parent);
    }

    insert_feedback_destroy(node);
    free(child);
    free(node);

    if (view->auto_balance != SPLIT_NONE) {
        window_node_balance(view->root, view->auto_balance);
        view_update(view);
        return view->root;
    }

    return parent;
}

void view_stack_window_node(struct window_node *node, struct window *window)
{
    int insert_index = node->window_count;

    for (int i = 0; i < node->window_count; ++i) {
        if (node->window_list[i] == node->window_order[0]) {
            insert_index = i+1;
            break;
        }
    }

    if (insert_index < node->window_count) {
        memmove(node->window_list + insert_index + 1, node->window_list + insert_index, sizeof(uint32_t) * (node->window_count - insert_index));
    }

    node->window_list[insert_index] = window->id;
    memmove(node->window_order + 1, node->window_order, sizeof(uint32_t) * node->window_count);
    node->window_order[0] = window->id;
    ++node->window_count;
}

struct window_node *view_add_window_node_with_insertion_point(struct view *view, struct window *window, uint32_t insertion_point)
{
    if (!window_node_is_occupied(view->root) &&
        window_node_is_leaf(view->root)) {
        view->root->window_list[0] = window->id;
        view->root->window_order[0] = window->id;
        view->root->window_count = 1;
        return view->root;
    } else if (view->layout == VIEW_BSP) {
        uint32_t prev_insertion_point = 0;
        struct window_node *leaf = NULL;

        if (insertion_point) {
            prev_insertion_point = view->insertion_point;
            view->insertion_point = insertion_point;
        }

        if (view->insertion_point) {
            leaf = view_find_window_node(view, view->insertion_point);
            view->insertion_point = prev_insertion_point;

            if (leaf) {
                bool do_stack = leaf->insert_dir == STACK;

                leaf->insert_dir = 0;
                insert_feedback_destroy(leaf);

                if (do_stack) {
                    view_stack_window_node(leaf, window);
                    return leaf;
                }
            }
        }

        if (!leaf) {
            if (g_space_manager.window_insertion_point == INSERT_FOCUSED) {
                leaf = view_find_window_node(view, g_window_manager.focused_window_id);
            } else if (g_space_manager.window_insertion_point == INSERT_FIRST) {
                leaf = window_node_find_first_leaf(view->root);
            } else if (g_space_manager.window_insertion_point == INSERT_LAST) {
                leaf = window_node_find_last_leaf(view->root);
            }

            if (!leaf) leaf = view_find_min_depth_leaf_node(view->root);
        }

        window_node_split(view, leaf, window);

        if (view->auto_balance != SPLIT_NONE) {
            window_node_balance(view->root, view->auto_balance);
            view_update(view);
            return view->root;
        }

        return leaf;
    } else if (view->layout == VIEW_STACK) {
        view_stack_window_node(view->root, window);
        return view->root;
    } else if (view->layout == VIEW_MAIN_STACK) {
        if (view->root->window_count >= NODE_MAX_WINDOW_COUNT) {
            return NULL;
        }

        int main_count = 0;
        for (int i = 0; i < view->root->window_count; i++) {
            if (view->root->window_regions[i] == REGION_MAIN) main_count++;
        }

        int idx = view->root->window_count;
        view->root->window_list[idx] = window->id;
        memmove(view->root->window_order + 1, view->root->window_order, sizeof(uint32_t) * view->root->window_count);
        view->root->window_order[0] = window->id;

        if (main_count < view->main_nmain) {
            view->root->window_regions[idx] = REGION_MAIN;
        } else {
            view->root->window_regions[idx] = REGION_STACK;
        }

        view->root->window_count++;
        view_update(view);
        view_flush(view);
        return view->root;
    }

    return NULL;
}

struct window_node *view_add_window_node(struct view *view, struct window *window)
{
    return view_add_window_node_with_insertion_point(view, window, 0);
}

uint32_t *view_find_window_list(struct view *view, int *window_count)
{
    *window_count = 0;

    int capacity = 13;
    uint32_t *window_list = ts_alloc_list(uint32_t, capacity);

    for (struct window_node *node = window_node_find_first_leaf(view->root); node; node = window_node_find_next_leaf(node)) {
        if (*window_count + node->window_count >= capacity) {
            ts_expand(window_list, sizeof(uint32_t) * capacity, sizeof(uint32_t) * capacity);
            capacity *= 2;
        }

        for (int i = 0; i < node->window_count; ++i) {
            window_list[(*window_count)++] = node->window_list[i];
        }
    }

    return window_list;
}

bool view_is_invalid(struct view *view)
{
    return !view_check_flag(view, VIEW_IS_VALID);
}

bool view_is_dirty(struct view *view)
{
    return view_check_flag(view, VIEW_IS_DIRTY);
}

void view_flush_main_stack(struct view *view)
{
    struct window_node *node = view->root;
    struct window_capture *window_list = NULL;

    if (node->window_count == 0) return;

    debug("view_flush_main_stack: window_count=%d, variant=%s\n",
          node->window_count, main_layout_variant_str[view->main_variant]);

    // Always assign regions (in case nmain or variant changed)
    assign_regions_to_windows(view, node);

    // Check each region - if any window in a region has ratio 0, reinitialize that region only
    bool main_needs_init = false, stack_needs_init = false;
    int main_count = 0, stack_count = 0;

    for (int i = 0; i < node->window_count; i++) {
        if (node->window_regions[i] == REGION_MAIN) {
            main_count++;
            if (node->window_ratios[i] < 0.01f) main_needs_init = true;
        } else {
            stack_count++;
            if (node->window_ratios[i] < 0.01f) stack_needs_init = true;
        }
    }

    // Reinitialize ratios for regions that need it
    if (main_needs_init || stack_needs_init) {
        debug("Reinitializing ratios: main=%d, stack=%d\n",
              main_needs_init, stack_needs_init);

        for (int i = 0; i < node->window_count; i++) {
            if (node->window_regions[i] == REGION_MAIN && main_needs_init) {
                node->window_ratios[i] = (main_count > 0) ? (1.0f / main_count) : 1.0f;
            } else if (node->window_regions[i] == REGION_STACK && stack_needs_init) {
                node->window_ratios[i] = (stack_count > 0) ? (1.0f / stack_count) : 1.0f;
            }
        }
    }

    struct area main_area = {0};
    struct area stack_left_area = {0};
    struct area stack_right_area = {0};

    area_make_main_stack(view, &node->area, &main_area, &stack_left_area, &stack_right_area, stack_count);

    debug("Areas: parent=(%.0f,%.0f,%.0fx%.0f) main=(%.0f,%.0f,%.0fx%.0f) left=(%.0f,%.0f,%.0fx%.0f) right=(%.0f,%.0f,%.0fx%.0f)\n",
          node->area.x, node->area.y, node->area.w, node->area.h,
          main_area.x, main_area.y, main_area.w, main_area.h,
          stack_left_area.x, stack_left_area.y, stack_left_area.w, stack_left_area.h,
          stack_right_area.x, stack_right_area.y, stack_right_area.w, stack_right_area.h);

    uint32_t main_windows[NODE_MAX_WINDOW_COUNT];
    uint32_t stack_left_windows[NODE_MAX_WINDOW_COUNT];
    uint32_t stack_right_windows[NODE_MAX_WINDOW_COUNT];
    float main_ratios[NODE_MAX_WINDOW_COUNT];
    float stack_left_ratios[NODE_MAX_WINDOW_COUNT];
    float stack_right_ratios[NODE_MAX_WINDOW_COUNT];

    // Collect windows and split stack between left/right based on variant
    int m_idx = 0, sl_idx = 0, sr_idx = 0;
    int stack_idx = 0;  // Index within stack windows

    for (int i = 0; i < node->window_count; i++) {
        if (node->window_regions[i] == REGION_MAIN) {
            main_windows[m_idx] = node->window_list[i];
            main_ratios[m_idx] = node->window_ratios[i];
            m_idx++;
        } else {
            // Stack window - decide left or right based on variant
            if (view->main_variant == MAIN_VARIANT_TWO_COLUMN) {
                // All stack windows go to the right
                stack_right_windows[sr_idx] = node->window_list[i];
                stack_right_ratios[sr_idx] = node->window_ratios[i];
                sr_idx++;
            } else if (view->main_variant == MAIN_VARIANT_THREE_COLUMN) {
                // First half of stack goes right, second half goes left
                // Right gets ceiling(stack_count / 2), left gets floor(stack_count / 2)
                int right_stack_count = (stack_count + 1) / 2;  // ceiling division

                if (stack_idx < right_stack_count) {
                    // First half → right stack
                    stack_right_windows[sr_idx] = node->window_list[i];
                    stack_right_ratios[sr_idx] = node->window_ratios[i];
                    sr_idx++;
                } else {
                    // Second half → left stack
                    stack_left_windows[sl_idx] = node->window_list[i];
                    stack_left_ratios[sl_idx] = node->window_ratios[i];
                    sl_idx++;
                }
            }
            stack_idx++;
        }
    }

    debug("Window distribution: main=%d, left=%d, right=%d\n",
          m_idx, sl_idx, sr_idx);

    int gap = window_node_get_gap(view);
    tile_windows_in_region(main_area, main_windows, main_ratios, m_idx, gap, &window_list);
    tile_windows_in_region(stack_left_area, stack_left_windows, stack_left_ratios, sl_idx, gap, &window_list);
    tile_windows_in_region(stack_right_area, stack_right_windows, stack_right_ratios, sr_idx, gap, &window_list);

    int capture_count = window_list ? ts_buf_len(window_list) : 0;
    debug("About to animate %d windows, window_list=%p\n", capture_count, (void*)window_list);

    if (window_list) {
        window_manager_animate_window_list(window_list, ts_buf_len(window_list));
        debug("Finished animating windows\n");
    } else {
        debug("ERROR: window_list is NULL!\n");
    }
}

bool view_promote_window_to_main(struct view *view, uint32_t window_id)
{
    if (view->layout != VIEW_MAIN_STACK) return false;

    struct window_node *node = view->root;
    int window_idx = -1;

    // Find the window
    for (int i = 0; i < node->window_count; i++) {
        if (node->window_list[i] == window_id) {
            window_idx = i;
            break;
        }
    }

    if (window_idx == -1) return false;

    // If already in main region, return false (already promoted)
    if (node->window_regions[window_idx] == REGION_MAIN) return false;

    // Increment main_nmain to add this window to main region
    view->main_nmain++;
    view_set_flag(view, VIEW_MAIN_NMAIN);

    view_update(view);
    view_flush(view);
    return true;
}

bool view_demote_window_from_main(struct view *view, uint32_t window_id)
{
    if (view->layout != VIEW_MAIN_STACK) return false;

    struct window_node *node = view->root;
    int window_idx = -1;

    // Find the window
    for (int i = 0; i < node->window_count; i++) {
        if (node->window_list[i] == window_id) {
            window_idx = i;
            break;
        }
    }

    if (window_idx == -1) return false;

    // If already in stack region, return false (already demoted)
    if (node->window_regions[window_idx] == REGION_STACK) return false;

    // Decrement main_nmain to move this window to stack region
    view->main_nmain--;
    view_set_flag(view, VIEW_MAIN_NMAIN);

    view_update(view);
    view_flush(view);
    return true;
}

void view_swap_main_and_stack(struct view *view)
{
    if (view->layout != VIEW_MAIN_STACK) return;

    struct window_node *node = view->root;

    // Find first main and first stack window
    int first_main = -1, first_stack = -1;
    for (int i = 0; i < node->window_count; i++) {
        if (node->window_regions[i] == REGION_MAIN && first_main == -1) {
            first_main = i;
        } else if (node->window_regions[i] != REGION_MAIN && first_stack == -1) {
            first_stack = i;
        }
        if (first_main != -1 && first_stack != -1) break;
    }

    if (first_main == -1 || first_stack == -1) return;

    // Swap them
    uint32_t temp_id = node->window_list[first_main];
    node->window_list[first_main] = node->window_list[first_stack];
    node->window_list[first_stack] = temp_id;

    uint8_t temp_region = node->window_regions[first_main];
    node->window_regions[first_main] = node->window_regions[first_stack];
    node->window_regions[first_stack] = temp_region;

    view_flush(view);
}

void view_flush(struct view *view)
{
    if (space_is_visible(view->sid)) {
        debug("view_flush: layout=%s, main_variant=%s, sid=%llu\n",
              view_type_str[view->layout],
              (view->layout == VIEW_MAIN_STACK) ? main_layout_variant_str[view->main_variant] : "N/A",
              view->sid);
        if (view->layout == VIEW_MAIN_STACK) {
            view_flush_main_stack(view);
        } else {
            window_node_flush(view->root);
        }
        view_clear_flag(view, VIEW_IS_DIRTY);
    } else {
        view_set_flag(view, VIEW_IS_DIRTY);
    }
}

void view_serialize(FILE *rsp, struct view *view, uint64_t flags)
{
    TIME_FUNCTION;

    if (flags == 0x0) flags |= ~flags;

    bool did_output = false;
    fprintf(rsp, "{\n");

    if (flags & SPACE_PROPERTY_ID) {
        fprintf(rsp, "\t\"id\":%lld", view->sid);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_UUID) {
        if (did_output) fprintf(rsp, ",\n");

        char *uuid = ts_cfstring_copy(view->uuid);
        fprintf(rsp, "\t\"uuid\":\"%s\"", uuid ? uuid : "<unknown>");
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_INDEX) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"index\":%d", space_manager_mission_control_index(view->sid));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_LABEL) {
        if (did_output) fprintf(rsp, ",\n");

        struct space_label *space_label = space_manager_get_label_for_space(&g_space_manager, view->sid);
        fprintf(rsp, "\t\"label\":\"%s\"", space_label ? space_label->label : "");
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_TYPE) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"type\":\"%s\"", view_type_str[view->layout]);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_DISPLAY) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"display\":%d", display_manager_display_id_arrangement(space_display_id(view->sid)));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_WINDOWS) {
        if (did_output) fprintf(rsp, ",\n");

        int window_count = 0;
        uint32_t *window_list = space_window_list(view->sid, &window_count, true);

        fprintf(rsp, "\t\"windows\":[");
        for (int i = 0; i < window_count; ++i) {
            if (i < window_count - 1) {
                fprintf(rsp, "%d, ", window_list[i]);
            } else {
                fprintf(rsp, "%d", window_list[i]);
            }
        }
        fprintf(rsp, "]");
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_FIRST_WINDOW) {
        if (did_output) fprintf(rsp, ",\n");

        struct window_node *first_leaf = window_node_find_first_leaf(view->root);
        fprintf(rsp, "\t\"first-window\":%d", first_leaf ? first_leaf->window_order[0] : 0);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_LAST_WINDOW) {
        if (did_output) fprintf(rsp, ",\n");

        struct window_node *last_leaf = window_node_find_last_leaf(view->root);
        fprintf(rsp, "\t\"last-window\":%d", last_leaf ? last_leaf->window_order[0] : 0);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_HAS_FOCUS) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"has-focus\":%s", json_bool(view->sid == g_space_manager.current_space_id));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_IS_VISIBLE) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"is-visible\":%s", json_bool(space_is_visible(view->sid)));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_IS_FULLSCREEN) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"is-native-fullscreen\":%s", json_bool(space_is_fullscreen(view->sid)));
        did_output = true;
    }

    if (view->layout == VIEW_MAIN_STACK) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"main-variant\":\"%s\"", main_layout_variant_str[view->main_variant]);
        fprintf(rsp, ",\n\t\"main-nmain\":%d", view->main_nmain);
        fprintf(rsp, ",\n\t\"main-ratio\":%.4f", view->main_ratio);
        fprintf(rsp, ",\n\t\"main-stack-max\":%d", view->main_stack_max);
    }

    fprintf(rsp, "\n}");
}

void view_update(struct view *view)
{
    uint32_t did = space_display_id(view->sid);
    CGRect frame = display_bounds_constrained(did, false);
    view->root->area = area_from_cgrect(frame);

    if (view_check_flag(view, VIEW_ENABLE_PADDING)) {
        view->root->area.x += view->left_padding;
        view->root->area.w -= (view->left_padding + view->right_padding);
        view->root->area.y += view->top_padding;
        view->root->area.h -= (view->top_padding + view->bottom_padding);
    }

    window_node_update(view, view->root);
    view_set_flag(view, VIEW_IS_VALID);
    view_set_flag(view, VIEW_IS_DIRTY);
}

struct view *view_create(uint64_t sid)
{
    struct view *view = malloc(sizeof(struct view));
    memset(view, 0, sizeof(struct view));

    view->root = malloc(sizeof(struct window_node));
    memset(view->root, 0, sizeof(struct window_node));

    view->sid = sid;
    view->uuid = SLSSpaceCopyName(g_connection, sid);

    view_set_flag(view, VIEW_ENABLE_PADDING);
    view_set_flag(view, VIEW_ENABLE_GAP);

    if (space_is_user(view->sid)) {
        if (!view_check_flag(view, VIEW_LAYOUT))         view->layout         = g_space_manager.layout;
        if (!view_check_flag(view, VIEW_TOP_PADDING))    view->top_padding    = g_space_manager.top_padding;
        if (!view_check_flag(view, VIEW_BOTTOM_PADDING)) view->bottom_padding = g_space_manager.bottom_padding;
        if (!view_check_flag(view, VIEW_LEFT_PADDING))   view->left_padding   = g_space_manager.left_padding;
        if (!view_check_flag(view, VIEW_RIGHT_PADDING))  view->right_padding  = g_space_manager.right_padding;
        if (!view_check_flag(view, VIEW_WINDOW_GAP))     view->window_gap     = g_space_manager.window_gap;
        if (!view_check_flag(view, VIEW_AUTO_BALANCE))   view->auto_balance   = g_space_manager.auto_balance;
        if (!view_check_flag(view, VIEW_SPLIT_TYPE))     view->split_type     = g_space_manager.split_type;
        if (!view_check_flag(view, VIEW_MAIN_VARIANT))   view->main_variant   = g_space_manager.main_variant;
        if (!view_check_flag(view, VIEW_MAIN_NMAIN))     view->main_nmain     = g_space_manager.main_nmain;
        if (!view_check_flag(view, VIEW_MAIN_STACK_MAX)) view->main_stack_max = g_space_manager.main_stack_max;
        if (!view_check_flag(view, VIEW_MAIN_RATIO))     view->main_ratio     = g_space_manager.main_ratio;
        view_update(view);
    } else {
        view->layout = VIEW_FLOAT;
    }

    return view;
}

void view_destroy(struct view *view)
{
    if (view->root) {
        if (view->root->left)  window_node_destroy(view->root->left);
        if (view->root->right) window_node_destroy(view->root->right);

        for (int i = 0; i < view->root->window_count; ++i) {
            window_manager_remove_managed_window(&g_window_manager, view->root->window_list[i]);
        }

        insert_feedback_destroy(view->root);
        memset(view->root, 0, sizeof(struct window_node));
    }
}

void view_clear(struct view *view)
{
    if (view->root) {
        if (view->root->left)  window_node_destroy(view->root->left);
        if (view->root->right) window_node_destroy(view->root->right);

        for (int i = 0; i < view->root->window_count; ++i) {
            window_manager_remove_managed_window(&g_window_manager, view->root->window_list[i]);
        }

        insert_feedback_destroy(view->root);
        memset(view->root, 0, sizeof(struct window_node));
        view_update(view);
    }
}
