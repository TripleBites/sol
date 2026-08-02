#ifndef SOL_UI_SCENE_TREE_H
#define SOL_UI_SCENE_TREE_H

#include "node.h"
#include "draw_list.h"
#include "input_event.h"
#include <stdbool.h>

typedef struct DeferredQueue DeferredQueue;

typedef struct {
    Node         *root;
    Node         *focused_control;
    Node         *hovered_control;
    DrawList     *draw_list;
    DeferredQueue *deferred_queue;
    bool          layout_dirty;
    bool          tree_changing;
} SceneTree;

SceneTree *scene_tree_create(void);
void       scene_tree_destroy(SceneTree *tree);

/* Set the root node. The tree takes ownership (refs it). */
void       scene_tree_set_root(SceneTree *tree, Node *root);

/* Run one frame: process → layout → draw. delta is in seconds. */
void       scene_tree_process(SceneTree *tree, float delta);
void       scene_tree_draw(SceneTree *tree);

/* Mark layout as needing recomputation */
void       scene_tree_mark_layout_dirty(SceneTree *tree);

/* Get the current draw list (valid after scene_tree_draw) */
DrawList  *scene_tree_get_draw_list(SceneTree *tree);

/* Input event routing */
void       scene_tree_input(SceneTree *tree, const UIInputEvent *ev);

/* Hit-test: find the deepest Control under a screen-space point.
   Returns NULL if no Control is hit. */
Node      *scene_tree_hit_test(const SceneTree *tree, Vec2 screen_pos);

/* Debug: print the tree structure */
void       scene_tree_print(const SceneTree *tree);

#endif /* SOL_UI_SCENE_TREE_H */
