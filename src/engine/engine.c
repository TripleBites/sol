#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include "sol.h"

static SDL_Window* window = NULL;
static VkInstance instance = VK_NULL_HANDLE;
static VkSurfaceKHR surface = VK_NULL_HANDLE;

bool sol_init(const char* title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    // 1. Tell SDL3 we are creating a Vulkan-compatible window
    window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    // 2. Query Vulkan extensions required by the target platform's windowing system
    Uint32 extension_count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);

    // 3. Create Vulkan Instance
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "SDL3 Vulkan Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions
    };

    if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
        SDL_Log("Failed to create Vulkan instance");
        return false;
    }

    // 4. Create Vulkan Surface linked to SDL3 Window
    if (!SDL_Vulkan_CreateSurface(window, instance, NULL, &surface)) {
        SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Vulkan Instance & Surface created successfully!");
    return true;
}

void sol_shutdown(void) {
    if (surface && instance) {
        vkDestroySurfaceKHR(instance, surface, NULL);
    }
    if (instance) {
        vkDestroyInstance(instance, NULL);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

/* --- Python C API Bindings --- */

static PyObject* py_sol_init(PyObject* self, PyObject* args) {
    const char* title;
    int width, height;
    if (!PyArg_ParseTuple(args, "sii", &title, &width, &height)) return NULL;
    if (!sol_init(title, width, height)) {
        PyErr_SetString(PyExc_RuntimeError, "Engine initialization failed");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* py_sol_shutdown(PyObject* self, PyObject* args) {
    sol_shutdown();
    Py_RETURN_NONE;
}

static PyMethodDef EngineMethods[] = {
    {"init", py_sol_init, METH_VARARGS, "Initialize Vulkan + SDL3"},
    {"shutdown", py_sol_shutdown, METH_NOARGS, "Shutdown Engine"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef sol_module = {
    PyModuleDef_HEAD_INIT, "sol", NULL, -1, EngineMethods
};

PyMODINIT_FUNC PyInit_engine(void) {
    return PyModule_Create(&sol_module);
}