#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    /* phase_01: 口令 1 个整数 = p1_compute(challenge)
     * p1_step2(hi, lo):  t2 = 5*(hi - lo) + 0x9e8d
     * p1_step0(lo, mid): t0 = (3*lo + mid) ^ 0x3802   mid = (challenge >> 16) & 0xffffffff
     * p1_step1(t0, t2):  答案 = (t0 + 8*t2) ^ 0xbc24
     * 全部按 32 位无符号宽度回绕。
     */
    uint32_t lo = (uint32_t)(challenge & UINT64_C(0xFFFFFFFF));
    uint32_t hi = (uint32_t)(challenge >> 32);
    uint32_t mid = (uint32_t)((challenge >> 16) & UINT64_C(0xFFFFFFFF));
    uint32_t t2 = UINT32_C(5) * (uint32_t)(hi - lo) + UINT32_C(0x9e8d);
    uint32_t t0 = (UINT32_C(3) * lo + mid) ^ UINT32_C(0x3802);
    values[0] = (t0 + UINT32_C(8) * t2) ^ UINT32_C(0xbc24);
    return write_answer(out, cap, values, count);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 6;
    /* phase_02: 6 个整数。
     * a[0] = ((C >> 33) & 0x7f) + 0x1d
     * 循环 i=1..5: a[i] = 3 * ( ((C >> (10 + 2*(i-1))) & 0x1f) + i ) ^ a[i-1] )
     * 按 32 位无符号回绕。
     */
    uint32_t prev = (uint32_t)(((challenge >> 33) & UINT64_C(0x7f)) + UINT64_C(0x1d));
    values[0] = prev;
    for (uint32_t i = 1; i < 6; ++i) {
        uint32_t t = (uint32_t)((challenge >> (10 + 2 * (i - 1))) & UINT64_C(0x1f));
        prev = UINT32_C(3) * ((t + i) ^ prev);
        values[i] = prev;
    }
    return write_answer(out, cap, values, count);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;
    /* phase_03: 2 个整数。
     * a[0] = C & 7                    （switch 选择值，先比较）
     * a[1] = p3_dispatch(sel, arg)    arg = ((C >> 3) ^ (C >> 32)) 低 32 位
     * 跳转表 0x405820 被打乱，逐项映射后：
     *   sel 0 -> case_2: 5*arg + 0x2b458
     *   sel 1 -> case_6: 5*arg + 0x4250
     *   sel 2 -> case_1: 5*arg + 0x1710
     *   sel 3 -> case_7: (arg ^ 0x47f4) + (arg >> 2)
     *   sel 4 -> case_3: (arg ^ 0xf4e0) + (arg >> 1)
     *   sel 5 -> case_0: (2*arg) ^ 0x7102
     *   sel 6 -> case_5: (4*arg) ^ 0xe548
     *   sel 7 -> case_4: 5*arg + 0x3e107
     */
    uint32_t sel = (uint32_t)(challenge & UINT64_C(7));
    uint32_t arg = (uint32_t)((challenge >> 3) ^ (challenge >> 32));
    uint32_t res;
    switch (sel) {
        case 0:  res = UINT32_C(5) * arg + UINT32_C(0x2b458); break;
        case 1:  res = UINT32_C(5) * arg + UINT32_C(0x4250);  break;
        case 2:  res = UINT32_C(5) * arg + UINT32_C(0x1710);  break;
        case 3:  res = (arg ^ UINT32_C(0x47f4)) + (arg >> 2); break;
        case 4:  res = (arg ^ UINT32_C(0xf4e0)) + (arg >> 1); break;
        case 5:  res = (UINT32_C(2) * arg) ^ UINT32_C(0x7102); break;
        case 6:  res = (UINT32_C(4) * arg) ^ UINT32_C(0xe548); break;
        default: res = UINT32_C(5) * arg + UINT32_C(0x3e107); break;
    }
    values[0] = sel;
    values[1] = res;
    return write_answer(out, cap, values, count);
}

/* p4_walk(n, lo, hi, k)：在 [lo, hi] 上对 n 做二分查找式递归（32 位回绕）
 *   mid = lo + (hi - lo) / 2
 *   mid == n          : 返回 (n + 0xa2) ^ k
 *   n <  mid（左支）  : 3 * walk(n, lo, mid-1, k) ^ (mid + k) ^ 0x12
 *   n >  mid（右支）  : 3 * walk(n, mid+1, hi, k) ^ (mid + k) ^ 0x1e
 */
static uint32_t p4_walk(uint32_t n, uint32_t lo, uint32_t hi, uint32_t k) {
    uint32_t mid = lo + (hi - lo) / UINT32_C(2);
    if (mid == n) {
        return (n + UINT32_C(0xa2)) ^ k;
    }
    uint32_t mix = mid + k;
    if (n < mid) {
        return (UINT32_C(3) * p4_walk(n, lo, mid - 1, k)) ^ mix ^ UINT32_C(0x12);
    }
    return (UINT32_C(3) * p4_walk(n, mid + 1, hi, k)) ^ mix ^ UINT32_C(0x1e);
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    /* phase_04: 1 个整数 = p4_walk(C & 0x7f, 0, 127, k)
     * k = ((C >> 7) ^ (C >> 32)) 低 32 位
     */
    uint32_t n = (uint32_t)(challenge & UINT64_C(0x7f));
    uint32_t k = (uint32_t)((challenge >> 7) ^ (challenge >> 32));
    values[0] = p4_walk(n, 0, UINT32_C(127), k);
    return write_answer(out, cap, values, count);
}

/* phase_05 的静态链表（从 bomb 的 .rodata 0x405ba0 起读出，按数组下标代替指针）
 * 节点结构 16 字节：next(8B) | a(4B) | b(4B)
 */
#define P5_NODE_COUNT 12
static const uint32_t p5_next[P5_NODE_COUNT] = {4, 6, 3, 9, 1, 8, 10, 5, 0, 11, 2, 0xFFFFFFFFu};
static const uint32_t p5_a[P5_NODE_COUNT] = {
    0x18u, 0x638u, 0x439u, 0x115u, 0x529u, 0x25eu,
    0x757u, 0x813u, 0xb56u, 0x938u, 0x30au, 0xa86u};
static const uint32_t p5_b[P5_NODE_COUNT] = {
    0x632fu, 0xdbf0u, 0xecf8u, 0x1d4fu, 0xeda2u, 0x2720u,
    0x5632u, 0x9956u, 0x5f57u, 0x8350u, 0xc976u, 0x8319u};
static const uint32_t p5_heads[8] = {7, 5, 8, 0, 4, 1, 6, 10};

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;
    /* p5_traverse: 从 heads[C&7] 出发，
     * acc = 3 * ( ((C>>6)^(C>>32))^0xa534 )   （32 位回绕）
     * 依次访问 head 及后 (C>>3)&3 个节点：acc = (acc + b) ^ a；out 记录最后访问的 a
     * 口令 = acc 后接 out
     */
    uint32_t v = (uint32_t)(((challenge >> 6) ^ (challenge >> 32)) ^ UINT64_C(0xa534));
    uint32_t acc = UINT32_C(3) * v;
    uint32_t node = p5_heads[(uint32_t)(challenge & UINT64_C(7))];
    uint32_t last_a = p5_a[node];
    acc += p5_b[node];
    acc ^= last_a;
    uint32_t steps = (uint32_t)((challenge >> 3) & UINT64_C(3));
    for (uint32_t i = 0; i < steps; ++i) {
        node = p5_next[node];
        acc = UINT32_C(3) * acc;
        acc += p5_b[node];
        acc ^= p5_a[node];
        last_a = p5_a[node];
    }
    values[0] = acc;
    values[1] = last_a;
    return write_answer(out, cap, values, count);
}

/* phase_06 的函数指针表 p6_ops[8]（0x405e00）：
 *   0: (2x + 0x647e) ^ 3x      1: (x ^ 0x4bb4) + (x << 5)
 *   2: (x + 0x1825) ^ (x >> 3) 3: (x ^ 0x4bb4) + (x << 5)
 *   4: (x ^ (x >> 5)) + 0x18c3 5: rol(x, 1) ^ 0xbdb7
 *   6: ror(x, 3)               7: ror(x, 3)
 */
static uint32_t p6_op(uint32_t idx, uint32_t x) {
    switch (idx) {
        case 0: return ((UINT32_C(2) * x + UINT32_C(0x647e)) ^ (UINT32_C(3) * x));
        case 1: case 3: return ((x ^ UINT32_C(0x4bb4)) + (x << 5));
        case 2: return ((x + UINT32_C(0x1825)) ^ (x >> 3));
        case 4: return ((x ^ (x >> 5)) + UINT32_C(0x18c3));
        case 5: return ((x << 1) | (x >> 31)) ^ UINT32_C(0xbdb7);
        default: return ((x >> 3) | (x << 29));
    }
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    /* p6_dispatch(C):
     * x   = (C >> 6) ^ (C >> 32)          低 32 位
     * s1  = C & 7                          第一次间接调用选 op
     * s2  = (C >> 3) & 7                   第二次（表项地址 = 0x405e00 + (C&0x38)）
     * 答案 = op(s2, op(s1, x) ^ 0x838b) + 0x95c9   （32 位回绕）
     */
    uint32_t x = (uint32_t)((challenge >> 6) ^ (challenge >> 32));
    uint32_t s1 = (uint32_t)(challenge & UINT64_C(7));
    uint32_t s2 = (uint32_t)((challenge >> 3) & UINT64_C(7));
    values[0] = p6_op(s2, p6_op(s1, x) ^ UINT32_C(0x838b)) + UINT32_C(0x95c9);
    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    /* secret_phase: 第七行 = "OPEN n1 n2 n3"，总长必须在 [36,163]（secret_gate:
     * strlen-0x24 <= 0x7f，且前 4 字节 == "OPEN"；"OPEN" 之后的部分按 %u %u %u 解析）。
     *   sel  = context & 3                       -> n1
     *   x    = (C ^ (C >> 32)) 低 32 位
     *   r8   = (ctx ^ (ctx >> 32)) 低 32 位
     *   z    = x ^ r8;  mix = (rol(z,1) + 0x88f9) ^ 0x6d97   （secret_mix）
     *   r9   = (sel * 0x88f9) ^ ((ctx >> 16) 低 32 位)
     *   n2   = mix ^ r9
     *   n3   = n2 ^ (r8 + sel + 0x6d97 + (x >> 1))
     * 全部 32 位无符号回绕。
     */
    uint32_t sel = (uint32_t)(context & UINT64_C(3));
    uint32_t x = (uint32_t)(challenge ^ (challenge >> 32));
    uint32_t r8 = (uint32_t)(context ^ (context >> 32));
    uint32_t z = x ^ r8;
    uint32_t mix = ((z << 1) | (z >> 31)) + UINT32_C(0x88f9);
    mix ^= UINT32_C(0x6d97);
    uint32_t r9 = (sel * UINT32_C(0x88f9)) ^ (uint32_t)(context >> 16);
    uint32_t n2 = mix ^ r9;
    uint32_t n3 = n2 ^ (r8 + sel + UINT32_C(0x6d97) + (x >> 1));
    /* secret_gate: secret_copy 把整行复制到缓冲区 r12，但 OPEN 检查在 r12+32
     * （cmpl $0x4e45504f,-0xb0(%rbp)），secret_phase 从 r12+36 起按 "%u %u %u" 解析。
     * 所以第七行 = 恰好 32 个字符的前缀 + "OPEN n1 n2 n3"，总长须在 [36,163]。
     */
    int len = snprintf(out, cap, "%-32s", "sky-maintenance-station");
    if (len < 0 || (size_t)len >= cap) return 1;
    len = snprintf(out + len, cap - (size_t)len, "OPEN %" PRIu32 " %" PRIu32 " %" PRIu32, sel, n2, n3);
    if (len < 0 || (size_t)len >= cap) return 1;
    return 0;
}
