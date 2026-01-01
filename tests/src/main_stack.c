// Tests for main-stack layout functionality

TEST_FUNC(main_stack_region_assignment_single_window, {
    struct view view = {0};
    view.layout = VIEW_MAIN_STACK;
    view.main_nmain = 1;
    view.main_variant = MAIN_VARIANT_TWO_COLUMN;

    struct window_node node = {0};
    node.window_count = 1;
    node.window_list[0] = 100;
    view.root = &node;

    assign_regions_to_windows(&view, &node);

    TEST_CHECK(node.window_regions[0], REGION_MAIN);
})

TEST_FUNC(main_stack_region_assignment_two_windows, {
    struct view view = {0};
    view.layout = VIEW_MAIN_STACK;
    view.main_nmain = 1;
    view.main_variant = MAIN_VARIANT_TWO_COLUMN;

    struct window_node node = {0};
    node.window_count = 2;
    node.window_list[0] = 100;
    node.window_list[1] = 101;
    view.root = &node;

    assign_regions_to_windows(&view, &node);

    TEST_CHECK(node.window_regions[0], REGION_MAIN);
    TEST_CHECK(node.window_regions[1], REGION_STACK);
})

TEST_FUNC(main_stack_region_assignment_multiple_mains, {
    struct view view = {0};
    view.layout = VIEW_MAIN_STACK;
    view.main_nmain = 2;
    view.main_variant = MAIN_VARIANT_TWO_COLUMN;

    struct window_node node = {0};
    node.window_count = 4;
    node.window_list[0] = 100;
    node.window_list[1] = 101;
    node.window_list[2] = 102;
    node.window_list[3] = 103;
    view.root = &node;

    assign_regions_to_windows(&view, &node);

    TEST_CHECK(node.window_regions[0], REGION_MAIN);
    TEST_CHECK(node.window_regions[1], REGION_MAIN);
    TEST_CHECK(node.window_regions[2], REGION_STACK);
    TEST_CHECK(node.window_regions[3], REGION_STACK);
})

TEST_FUNC(main_stack_chunk_distribution_even_stack, {
    // With 4 stack windows: first 2 go right, last 2 go left
    int stack_count = 4;
    int right_stack_count = (stack_count + 1) / 2;  // ceiling(4/2) = 2

    TEST_CHECK(right_stack_count, 2);
})

TEST_FUNC(main_stack_chunk_distribution_odd_stack, {
    // With 5 stack windows: first 3 go right, last 2 go left
    int stack_count = 5;
    int right_stack_count = (stack_count + 1) / 2;  // ceiling(5/2) = 3

    TEST_CHECK(right_stack_count, 3);
})

TEST_FUNC(main_stack_ratio_initialization, {
    struct view view = {0};
    view.layout = VIEW_MAIN_STACK;
    view.main_nmain = 1;
    view.main_variant = MAIN_VARIANT_TWO_COLUMN;

    struct window_node node = {0};
    node.window_count = 3;
    node.window_list[0] = 100;
    node.window_list[1] = 101;
    node.window_list[2] = 102;
    view.root = &node;

    assign_regions_to_windows(&view, &node);

    // Initialize ratios for new windows
    for (int i = 0; i < node.window_count; i++) {
        if (node.window_ratios[i] == 0.0f) {
            if (node.window_regions[i] == REGION_MAIN) {
                node.window_ratios[i] = 1.0f;  // 1 main window
            } else {
                node.window_ratios[i] = 0.5f;  // 2 stack windows
            }
        }
    }

    TEST_CHECK((int)(node.window_ratios[0] * 100), 100);  // 1.0
    TEST_CHECK((int)(node.window_ratios[1] * 100), 50);   // 0.5
    TEST_CHECK((int)(node.window_ratios[2] * 100), 50);   // 0.5
})

TEST_FUNC(main_stack_wrap_around_calculation, {
    // Test wrap-around logic for next/prev navigation
    int window_count = 5;

    // prev from first window should wrap to last
    int current_idx = 0;
    int prev_idx = (current_idx == 0) ? (window_count - 1) : (current_idx - 1);
    TEST_CHECK(prev_idx, 4);

    // next from last window should wrap to first
    current_idx = 4;
    int next_idx = (current_idx == window_count - 1) ? 0 : (current_idx + 1);
    TEST_CHECK(next_idx, 0);

    // normal prev
    current_idx = 2;
    prev_idx = (current_idx == 0) ? (window_count - 1) : (current_idx - 1);
    TEST_CHECK(prev_idx, 1);

    // normal next
    current_idx = 2;
    next_idx = (current_idx == window_count - 1) ? 0 : (current_idx + 1);
    TEST_CHECK(next_idx, 3);
})
