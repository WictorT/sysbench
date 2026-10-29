/*
 * OpenCL (GPU) version of sysbench_prime.c.
 * Each GPU work-item runs one full "event": count primes in [3, max_prime) by trial
 * division up to sqrt(c). The host launches batches of work-items until the time limit.
 *
 * Build: gcc -O2 -Iinclude -o sysbench_prime_gpu.exe sysbench_prime_gpu.c "C:/Windows/System32/OpenCL.dll"
 * Usage: ./sysbench_prime_gpu.exe [max_prime] [seconds] [work_items]
 *
 * max_prime must fit in 32 bits (the kernel uses uint, 64-bit modulo is very slow on GPUs).
 */
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *kernel_src =
  "#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n"
  "__kernel void cpu_event(uint max_prime, __global uint *out)\n"
  "{\n"
  "  uint c, l, n = 0;\n"
  "  double t;\n"
  "  for (c = 3; c < max_prime; c++)\n"
  "  {\n"
  "    t = sqrt((double)c);\n"
  "    for (l = 2; l <= t; l++)\n"
  "      if (c % l == 0)\n"
  "        break;\n"
  "    if (l > t)\n"
  "      n++;\n"
  "  }\n"
  "  out[get_global_id(0)] = n;\n"
  "}\n";

static double now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

#define CHECK(call)                                                      \
  do {                                                                   \
    cl_int e_ = (call);                                                  \
    if (e_ != CL_SUCCESS) {                                              \
      fprintf(stderr, "%s failed: OpenCL error %d\n", #call, e_);        \
      exit(1);                                                           \
    }                                                                    \
  } while (0)

int main(int argc, char **argv)
{
  unsigned long long max_prime = 10000;
  double seconds = 10.0;
  size_t items = 8192;

  if (argc > 1)
    max_prime = strtoull(argv[1], NULL, 10);
  if (argc > 2)
    seconds = atof(argv[2]);
  if (argc > 3)
    items = strtoull(argv[3], NULL, 10);
  if (items < 1)
    items = 1;
  if (max_prime > 0xFFFFFFFFULL)
  {
    fprintf(stderr, "max_prime must be below 2^32\n");
    return 1;
  }

  cl_int err;
  cl_platform_id platform;
  cl_device_id device = NULL;
  cl_uint nplat;
  CHECK(clGetPlatformIDs(0, NULL, &nplat));
  cl_platform_id *plats = malloc(nplat * sizeof(*plats));
  CHECK(clGetPlatformIDs(nplat, plats, NULL));
  for (cl_uint i = 0; i < nplat && !device; i++)
    if (clGetDeviceIDs(plats[i], CL_DEVICE_TYPE_GPU, 1, &device, NULL) == CL_SUCCESS)
      platform = plats[i];
  if (!device)
  {
    fprintf(stderr, "No OpenCL GPU found\n");
    return 1;
  }
  (void)platform;

  char name[256];
  CHECK(clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, NULL));

  cl_context ctx = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
  CHECK(err);
  cl_command_queue q = clCreateCommandQueue(ctx, device, 0, &err);
  CHECK(err);
  cl_program prog = clCreateProgramWithSource(ctx, 1, &kernel_src, NULL, &err);
  CHECK(err);
  if (clBuildProgram(prog, 1, &device, NULL, NULL, NULL) != CL_SUCCESS)
  {
    char log[4096];
    clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
    fprintf(stderr, "Kernel build failed:\n%s\n", log);
    return 1;
  }
  cl_kernel k = clCreateKernel(prog, "cpu_event", &err);
  CHECK(err);

  cl_mem buf = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, items * sizeof(cl_uint), NULL, &err);
  CHECK(err);
  cl_uint mp = (cl_uint)max_prime;
  CHECK(clSetKernelArg(k, 0, sizeof(mp), &mp));
  CHECK(clSetKernelArg(k, 1, sizeof(buf), &buf));

  unsigned long long events = 0;
  double start = now();
  do
  {
    CHECK(clEnqueueNDRangeKernel(q, k, 1, NULL, &items, NULL, 0, NULL, NULL));
    CHECK(clFinish(q));
    events += items;
  } while (now() - start < seconds);
  double elapsed = now() - start;

  cl_uint *res = malloc(items * sizeof(*res));
  CHECK(clEnqueueReadBuffer(q, buf, CL_TRUE, 0, items * sizeof(*res), res, 0, NULL, NULL));
  size_t bad = 0;
  for (size_t i = 1; i < items; i++)
    if (res[i] != res[0])
      bad++;

  printf("Device: %s\n", name);
  printf("Prime numbers limit: %llu\n", max_prime);
  printf("Work-items per batch: %zu\n", items);
  printf("Primes found per event: %u%s\n", res[0], bad ? " (MISMATCH between work-items!)" : "");
  printf("Total events: %llu\n", events);
  printf("Total time: %.4fs\n", elapsed);
  printf("events per second: %.2f\n", events / elapsed);

  free(res);
  free(plats);
  clReleaseMemObject(buf);
  clReleaseKernel(k);
  clReleaseProgram(prog);
  clReleaseCommandQueue(q);
  clReleaseContext(ctx);
  return bad ? 1 : 0;
}
