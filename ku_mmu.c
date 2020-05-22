#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

/* virtual address mask, shift */
#define PD_MASK 0b11000000
#define PMD_MASK 0b00110000
#define PT_MASK 0b00001100
#define PO_MASK 0b00000011
#define PD_SHIFT 6
#define PMD_SHIFT 4
#define PT_SHIFT 2
#define PO_SHIFT 0

/* entry mask, shift */
#define PRESENT_BIT_MASK 0b00000001
#define PFN_MASK 0b11111100
#define SPN_MASK 0b11111110
#define PFN_SHIFT 2
#define SPN_SHIFT 1

/* page type */
#define NOT_USED_TYPE -1
#define P_TYPE_UNDEFINED 0
#define PD_TYPE 1
#define PMD_TYPE 2
#define PT_TYPE 3
#define PF_TYPE 4



/* page */
typedef struct page_ {
    char pte[4];
} Page;

/* 
    page free info
    : 페이지들이 free 한가를 판단하기 위한 정보를 저장한다.
        - page: 해당 page 시작 주소
        - type: page 의 타입 (PageDir/PageMidDir/PageTable/PageFrame)
        - is_free: 해당 page 가 free 할 경우 1 아니면 0
*/
typedef struct page_free_info_ {
    struct page_* page;
    char type;
    char is_free;
} PFRI;

/*
    page frame info (node)
    : PageFrame 을 swap 할 때 필요한 정보를 저장한다.
        - page: 해당 page 시작 주소
        - next: 다음 page (node 로 사용하기 위함)
        - pid: 해당 page 에 접근한 process 의 id
        - fadd: 해당 페이지가 대응하는 가상메모리 시작 주소 (first address)
        - ladd: 해당 페이지가 대응하는 가상메모리 마지막 주소 (last address)
*/
typedef struct page_frame_info_ {
    struct page_* page;
    struct page_* pgtable;
    struct page_frame_info_* next;
    int pfn;
    char ptenti;  // PT entry index
    char pid;
    unsigned char fadd;
    unsigned char ladd;
} PGF;

/* 
    page frame info queue
    : PGF 를 노드로 하는 리스트 구조체
*/
typedef struct page_frame_info_queue_ {
    struct page_frame_info_* head;
    struct page_frame_info_* tail;
    int len;
} PGF_Queue;

/*
    swap page info
    : swap 공간에 배정될 page 들의 정보를 저장하는 구조체
        - page: 해당 page 의 스왑공간 시작 주소
        - pid: 해당 page 에 접근한 process 의 id
        - fadd: 해당 페이지가 대응하는 가상메모리 시작 주소 (first address)
        - ladd: 해당 페이지가 대응하는 가상메모리 마지막 주소 (last address)
*/
typedef struct swap_page_info_ {
    struct page_* page;
    struct page_* pgtable;
    int spn;
    char ptenti;
    char pid;
    unsigned char fadd;
    unsigned char ladd;
    char is_free;
} SPI;

/*
    Process Control Block (node)
    : 프로세스를 관리하기 위한 정보를 담는 구조체
        - pgdir: 해당 process 의 PageDir 시작 주소
        - next: 다음 PCB 시작 주소
        - pid: process id
*/
typedef struct pcb_ {
    struct page_* pgdir;
    // struct page_ **cr3;
    struct pcb_* next;
    char pid;
} PCB;

/*
    PCB List
    : PCB 를 노드로 하는 단방향 리스트 구조체
*/
typedef struct pcblist_ {
    struct pcb_* head;
    struct pcb_* tail;
    int len;
} PCB_List;

int pfl_sz;  // pg_free_list 의 사이즈
int spl_sz;  // sp_list 의 사이즈
PFRI* pg_free_list;  // 물리 메모리 영역의 페이지들이 free 한 상태인지 여부가 담긴 배열 포인터
SPI* sp_list;  // 스왑 영역의 페이지들의 정보를 담은 배열 포인터
PGF_Queue* pgf_queue;  // 할당된 PageFrame 이 순서대로 저장된 단방향 연결리스트 포인터
PCB_List* pcb_list;  // ProcessControlBlock 단방향 연결리스트 포인터



/*
    Page 관련 함수 
*/
void setZeroPage(Page* page) {
    if (!page) return;
    page->pte[0] = 0;
    page->pte[1] = 0;
    page->pte[2] = 0;
    page->pte[3] = 0;
}

void copyPage(Page* from, Page* to) {
    to->pte[0] = from->pte[0];
    to->pte[1] = from->pte[1];
    to->pte[2] = from->pte[2];
    to->pte[3] = from->pte[3];
}



/*
    PGF 를 노드로 하는 PGF_Queue 를 다루기 위한 함수들
*/
PGF* createPGF(Page* page, Page* pgtable, int pfn, char ptenti, char pid, unsigned char add) {
    PGF* pfi = (PGF *)malloc(sizeof(PGF));
    pfi->page = page;
    pfi->pgtable = pgtable;
    pfi->pfn = pfn;
    pfi->ptenti = ptenti;
    pfi->pid = pid;
    pfi->fadd = (add >> 2) << 2;
    pfi->ladd = pfi->fadd + 3;
    pfi->next = NULL;
    return pfi;
}

PGF* addPGF(PGF_Queue* l, Page* page, Page* pgtable, int pfn, char ptenti, char pid, unsigned char add) {
    PGF* pfi = createPGF(page, pgtable, pfn, ptenti, pid, add);
    if (l->head == NULL) l->head = l->tail = pfi;
    else {
        l->tail->next = pfi;
        l->tail = pfi;
    }
    l->len++;
    return pfi;
}

PGF* popHeadPGF(PGF_Queue* l) {
    PGF* curr = l->head;
    if (curr == NULL) return NULL;
    l->head = curr->next;
    curr->next = NULL;
    l->len--;
    return curr;
}

void insertHeadPGF(PGF_Queue* l, PGF* pgf) {
    if (pgf == NULL) return;
    pgf->next = l->head;
    l->head = pgf;
    l->len++;
}



/*
    SPI 를 다루기 위한 함수들
*/
SPI* createSPI() {
    SPI* spi = (SPI*)malloc(sizeof(SPI));
    spi->page = (Page*)malloc(sizeof(Page));
    return spi;
}

void copySPI(SPI* from, SPI* to) {
    copyPage(from->page, to->page);
    to->pgtable = from->pgtable;
    to->spn = from->spn;
    to->ptenti = from->ptenti;
    to->pid = from->pid;
    to->fadd = from->fadd;
    to->ladd = from->ladd;
    to->is_free = FALSE;
}



/*
    PCB 를 노드로 하는 PCB_List 를 다루기 위한 함수들
*/
PCB* createPCB(char pid) {
    PCB* pcb = (PCB *)malloc(sizeof(PCB));
    pcb->pid = pid;
    pcb->pgdir = NULL;
    pcb->next = NULL;
    return pcb;
}

PCB* addPCB(PCB_List* l, char pid) {
    PCB* npcb = createPCB(pid);
    if (l->head == NULL) l->head = l->tail = npcb;
    else {
        l->tail->next = npcb;
        l->tail = npcb;
    }
    l->len++;
    return npcb;
}

void removeTailPCB(PCB_List* l) {
    PCB* curr = l->tail;
    if (curr == NULL) return;
    while (curr->next == l->tail) {
        curr = curr->next;
    }
    curr->next = NULL;
    free(l->tail);
    l->tail = curr;
    l->len--;
}

PCB* searchPCB(PCB_List* l, char pid) {
    PCB* curr = l->head;
    while (curr != NULL) {
        if (curr->pid == pid) return curr;
        curr = curr->next;
    }
    return NULL;
}

void freePCBList(PCB_List* l) {
    PCB* curr = l->head;
    PCB* temp;
    while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp->pgdir);
        free(temp);
    }
}



/*
    print 함수 
*/
void pt_entry(char ent) {
    int p = ent & PRESENT_BIT_MASK;
    int pfn = (ent & PFN_MASK) >> PFN_SHIFT;
    int spn = (ent & SPN_MASK) >> SPN_SHIFT;
    if (p || pfn || spn)
        printf("(%d, %2d, %2d) ", p, pfn, spn);
    else
        printf("(         ) ");
}

void pt_pg_free_list() {
    printf("  pg_free_list = [ \n");
    for (int i = 0; i < pfl_sz; ++i) {
        if (pg_free_list[i].page) 
            printf("\t%2d (page: %p, ", i, pg_free_list[i].page);
        else
            printf("\t%2d (page: NULL,           ", i);
        printf("type: %2d, ", pg_free_list[i].type);
        printf("is_free: %d) ", pg_free_list[i].is_free);
        if (pg_free_list[i].page) {
            printf(
                "entry: %2x %2x %2x %2x -> ", 
                pg_free_list[i].page->pte[0],
                pg_free_list[i].page->pte[1],
                pg_free_list[i].page->pte[2],
                pg_free_list[i].page->pte[3]
            );
            printf("p, pfn, spn: ");
            pt_entry(pg_free_list[i].page->pte[0]);
            pt_entry(pg_free_list[i].page->pte[1]);
            pt_entry(pg_free_list[i].page->pte[2]);
            pt_entry(pg_free_list[i].page->pte[3]);
        }
        else
            printf("               ");
        printf("\n");
    }
    printf("  ]\n");
}

void pt_sp_list() {
    printf("  sp_list = [ \n");
    for (int i = 0; i < spl_sz; ++i) {
        if (sp_list[i].page)
            printf("\t%2d (page: %p, ", i, sp_list[i].page);
        else
            printf("\t%2d (page: NULL,           ", i);
        printf("pid: %2d, ", sp_list[i].pid);
        printf("fadd: %3d, ", sp_list[i].fadd);
        printf("ladd: %3d, ", sp_list[i].ladd);
        printf("is_free: %d) ", sp_list[i].is_free);
        if (sp_list[i].page) {
            printf(
                "entry: %2x %2x %2x %2x -> ", 
                sp_list[i].page->pte[0],
                sp_list[i].page->pte[1],
                sp_list[i].page->pte[2],
                sp_list[i].page->pte[3]
            );
            printf("p, pfn, spn: ");
            pt_entry(sp_list[i].page->pte[0]);
            pt_entry(sp_list[i].page->pte[1]);
            pt_entry(sp_list[i].page->pte[2]);
            pt_entry(sp_list[i].page->pte[3]);
        }
        else
            printf("\t\t\t\t\t\t\t\t\t              ");
        if (sp_list[i].pgtable) {
            printf("(pgtable: %p, ", sp_list[i].pgtable);
            printf("ptenti: %d, ", sp_list[i].ptenti);
            printf("pgtable entry: %2x)", sp_list[i].pgtable->pte[(int)sp_list[i].ptenti]);
        }
        else
            printf("(pgtable: NULL)");
        printf("\n");
    }
    printf("  ]\n");
}

void pt_pgf_queue() {
    PGF* curr = pgf_queue->head;
    int i = 0;
    printf("  pgf_queue = [");
    if (curr) printf("\n");
    while (curr != NULL) {
        if (curr->page)
            printf("\t%2d (page: %p, ", i, curr->page);
        else
            printf("\t%2d (page: NULL,           ", i);
        printf("pid: %2d, ", curr->pid);
        printf("fadd: %3d, ", curr->fadd);
        printf("ladd: %3d, ", curr->ladd);
        printf("pfn: %2d) ", curr->pfn);
        if (curr->page) {
            printf(
                "entry: %2x %2x %2x %2x -> ", 
                curr->page->pte[0],
                curr->page->pte[1],
                curr->page->pte[2],
                curr->page->pte[3]
            );
            printf("p, pfn, spn: ");
            pt_entry(curr->page->pte[0]);
            pt_entry(curr->page->pte[1]);
            pt_entry(curr->page->pte[2]);
            pt_entry(curr->page->pte[3]);
        }
        else
            printf("               ");
        if (curr->pgtable) {
            printf("(pgtable: %p, ", curr->pgtable);
            printf("ptenti: %d, ", curr->ptenti);
            printf("pgtable entry: %2x)", curr->pgtable->pte[(int)curr->ptenti]);
        }
        else
            printf("(pgtable: NULL)          ");
        printf("\n");
        i++;
        curr = curr->next;
    }
    printf("  ]\n");
}

void pt_pcb_list() {
    PCB* curr = pcb_list->head;
    int i = 0;
    printf("  pcb_list = [");
    if (curr) printf("\n");
    while (curr != NULL) {
        printf("\t%2d (pgdir: %p, ", i, curr->pgdir);
        printf("pid: %2d)\n", curr->pid);
        i++;
        curr = curr->next;
    }
    printf("  ]\n");
}




/*
    메인 함수에 반복적으로 필요한 기능을 모듈화한 함수들
*/
int getFreePage(char type) {
    /*
        : pg_free_list 를 순회하면서, Free Page 가 있으면 해당 page 의 시작 주소를 반환하고
        없으면 NULL 을 반환하는 함수
        
        type: 반환될 page가 사용될 타입 (PageDir/PageMidDir/PageTable/PageFrame)

        pg_free_list 순회하면서
            - free page 가 있으면
                - pg_free_list[i].type = type
                - pg_free_list[i].is_free = FALSE
                - return pg_free_list[i].page
            - free page 가 없으면 
                - return NULL
    */
    for (int i = 1; i < pfl_sz; ++i) {
        if (pg_free_list[i].is_free) {
            pg_free_list[i].type = type;
            pg_free_list[i].is_free = FALSE;
            return i;
        }
    }
    return 0;
}

PGF* getPageFrame() {
    /*
        pgf_queue 의 head 를 반환하는 함수 (형식을 맞추기 위해 사용)
    */
    return pgf_queue->head;
}

SPI* getFreeSwapPage() {
    /*
        스왑 영역의 남는 페이지를 SPI 타입으로 반환
    */
    for (int i = 1; i < spl_sz; ++i) {
        if (sp_list[i].is_free) return sp_list + i;
    }
    return NULL;
}

SPI* getSwapPage(char pid, unsigned char add) {
    /*
        (pid, address) 쌍에 부합하는 스왑페이지가 있으면 반환. 없으면 NULL 반환
    */
    SPI* spi = createSPI();
    for (int i = 1; i < spl_sz; ++i) {
        if (sp_list[i].pid == pid) {
            if (sp_list[i].fadd <= add && add <= sp_list[i].ladd) {
                sp_list[i].is_free = TRUE;
                copySPI(sp_list + i, spi);
                return spi;
            }
        }
    }
    return NULL;
}

void swapIn(SPI* spi, int pfn) {
    /*
        스왑 페이지의 정보를 pg_free_list 에 저장한다.
        해당 페이지와 관련된 PageFrame 과 PageTable 의 정보도 갱신한다.
    */
    // page 내용 복사
    Page* page = pg_free_list[pfn].page;
    copyPage(spi->page, page);
    // PF 업데이트
    addPGF(pgf_queue, page, spi->pgtable, pfn, spi->ptenti, spi->pid, spi->fadd);
    // PT 업데이트
    spi->pgtable->pte[(int)spi->ptenti] = (pfn << 2) + PRESENT_BIT_MASK;
}

void swapOut(PGF* pgf, SPI* spi) {    
    /*
        PageFrame 정보를 스왑 페이지에 저장.
        관련된 PageTable 을 갱신하고, PageFrame 구조체 포인터를 free 한다.
    */
    // swap 공간에 복사
    copyPage(pgf->page, spi->page);
    spi->pgtable = pgf->pgtable;
    spi->ptenti = pgf->ptenti;
    spi->pid = pgf->pid;
    spi->fadd = pgf->fadd;
    spi->ladd = pgf->ladd;
    spi->is_free = FALSE;
    // pgf 의 page 초기화
    setZeroPage(pgf->page);
    pgf->pgtable->pte[(int)pgf->ptenti] = (spi->spn << SPN_SHIFT);
    free(pgf);
}



/*
    ku_mmc.h 의 핵심 함수들
*/
int addPage(char type) {
    /*
        free page 나, 스왑 가능한 page 를 알아서 처리후 사용 가능한 page 의 pfn 반환.
        사용 가능한 page 가 없을 경우 0 반환. 
        (page[0] 은 NULL 값으로 초기화되어 있고, 그 후에 변경되지 않기 때문에, 나중에 fail 처리가 가능)
    */
    int pfn = getFreePage(type);
    Page* npage = pg_free_list[pfn].page;
    SPI* spi = getFreeSwapPage();
    PGF* pgf = getPageFrame();
    // free page 가 있을 때
    if (npage) {
        setZeroPage(npage);
    }
    // PageFrame 이나 SwapSpace 중 하나라도 없으면 fail
    else if (pgf == NULL || spi == NULL) return 0;  // fail
    // PageFrame 과 SwapSpace 둘 다 있을 때
    else {
        popHeadPGF(pgf_queue);
        swapOut(pgf, spi);
        pfn = pgf->pfn;
        pg_free_list[pfn].type = type;
    }
    return pfn;
}

int penetrate(char pid, char va) {
    /*
        :return: 중간에 불가능한 상황이 오면 -1, 정상 종료는 0 리턴
    */
    int ent, p, pfn, spn;
    PCB* pcb = searchPCB(pcb_list, pid);
    Page* lpage;
    char enti[4];
    char mask[4] = { PD_MASK, PMD_MASK, PT_MASK, PO_MASK };
    char shift[4] = { PD_SHIFT, PMD_SHIFT, PT_SHIFT, PO_SHIFT };
    char type[3] = { PMD_TYPE, PT_TYPE, PF_TYPE };

    for (int i = 0; i < 4; ++i) {
        // enti[4] = { pdi, pmdi, pti, pfi }
        enti[i] = ((unsigned char)va & mask[i]) >> shift[i];
    }
    

    lpage = pcb->pgdir;
    for (int i = 0; i < 3; ++i) {
        ent = lpage->pte[(int)enti[i]];
        // PageMidDir PFN 구하기
        p = ent & PRESENT_BIT_MASK;
        pfn = (ent & PFN_MASK) >> PFN_SHIFT;
        spn = (ent & SPN_MASK) >> SPN_SHIFT;
        if (p) {
            /* 매핑 된 상태 */
            lpage = pg_free_list[pfn].page;
        }
        else if (spn) {
            /* 스왑된 상태 */
            if (i != 2) {
                printf("ERROR: PD, PMD, PT 중의 하나가 스왑되어 있습니다.\n");
                return -1;
            }
            // 스왑 페이지 가져오기
            SPI* spi = getSwapPage(pcb->pid, (unsigned char)va);
            pfn = addPage(type[i]);
            swapIn(spi, pfn);
        }
        else {
            /* 접근한 적 없는 상태 (lpage 의 해당 엔트리가 비어있는 상태) */
            pfn = addPage(type[i]);
            Page* npage = pg_free_list[pfn].page;
            // 새로 만들 수 없는 경우 fail
            if (npage == NULL) return -1;
            // 이전 페이지의 엔트리 업데이트
            lpage->pte[(int)enti[i]] = (pfn << 2) + PRESENT_BIT_MASK;
            if (i == 2) {  // PageTable 에 PageFrame 을 추가할 때 -> pgf_queue 업데이트
                addPGF(pgf_queue, npage, lpage, pfn, enti[i], pcb->pid, (unsigned char)va);
            }
            lpage = npage;
        }
    }
    return 0;
}

int ku_page_fault (char pid, unsigned char va) {
    /*
        pid: page fault 가 발생한 프로세스의 id
        va: page fault 가 발생한 Virtual Address

        전제조건
            - pid 의 process 가 돌아가고 있고
            - va 접근이 page fault 가 난 상태

        - 접근하려는 주소: 11 00 10 11
        - PageDir index: 11
        - PageMidDir index: 00
        - PageTable index: 10
        - Offset: 11
        
        2. PageDir index: 11 -> ku_cr3 를 기준점으로 3번째 pte 값을 가져와서, 
            PageMiddleDir 의 PFN 을 가져온다. 
            (예: 00 00 11 01 에서 PFN = 3 을 가져온다)
        
        3. pmem 을 기준으로 PFN = 3 인 페이지의 시작 주소를 계산한다 (예: 0x4830 + 3*4 = 0x483c)
        4. PageMiddleDir index: 00 -> PFN = 3 인 페이지의 0 번째 pte 값을 가져와서, 
            PageTable 의 PFN 을 가져온다 
            (예: PFN3 의 0 번 엔트리가 00 01 01 01 일 때, PFN = 5 를 가져온다)
        
        5. pmem 을 기준으로 PFN = 5 인 페이지의 시작 주소를 계산한다 (예: 0x4830 + 5*4 = 0x4844)
        6. PageTable index: 10 -> PFN = 5 인 페이지의 2 번째 pte 값을 가져와서, 
            마지막 PageFrame 의 PFN 을 가져온다 
            (예: PFN5 의 2 번 엔트리가 00 10 01 01 일 때, PFN = 9 를 가져온다)
        
        7. pmem 을 기준으로 PFN = 9 인 페이지의 시작 주소를 계산한다 (예: 0x4830 + 9*4 = 0x4854)
        8. Offset: 11 -> 4번의 PFN 이 가리키는 페이지의 3 번째 pte 가 접근하고자 하는 주소이다. 
            (예: PFN9 의 3번째 엔트리가 접근하고자 하는 주소)

        하는 일
            - 접근한 적이 없을 때
            - 스왑되어 있는 상태일 때
                - 스왑 인
                    - free list 가 있을 때
                        - getFreePage
                        - swap in
                    - ㅖageFrame 이 있을 때
                        - swap in
                        - swap out
    */
    
    int ent, p, pfn, spn;
    PCB* pcb = searchPCB(pcb_list, pid);
    Page* lpage;
    char enti[4];
    char mask[4] = { PD_MASK, PMD_MASK, PT_MASK, PO_MASK };
    char shift[4] = { PD_SHIFT, PMD_SHIFT, PT_SHIFT, PO_SHIFT };
    char type[3] = { PMD_TYPE, PT_TYPE, PF_TYPE };

    printf("\n===============================\n");
    printf("<ku_page_fault(pid: %d, va: %d)>\n", pid, (unsigned char)va);

    for (int i = 0; i < 4; ++i) {
        // enti[4] = { pdi, pmdi, pti, pfi }
        enti[i] = ((unsigned char)va & mask[i]) >> shift[i];
    }

    lpage = pcb->pgdir;
    for (int i = 0; i < 3; ++i) {
        ent = lpage->pte[(int)enti[i]];
        // PageMidDir PFN 구하기
        p = ent & PRESENT_BIT_MASK;
        pfn = (ent & PFN_MASK) >> PFN_SHIFT;
        spn = (ent & SPN_MASK) >> SPN_SHIFT;
        if (p) {
            /* 매핑 된 상태 */
            lpage = pg_free_list[pfn].page;
        }
        else if (spn) {
            /* 스왑된 상태 */
            if (i != 2) {
                printf("ERROR: PD, PMD, PT 중의 하나가 스왑되어 있습니다.\n");
                return -1;
            }
            // 스왑 페이지 가져오기

            printf("\nswap in 시도: (pid: %d, va: %d)", pid, (unsigned char)va);

            SPI* spi = getSwapPage(pcb->pid, (unsigned char)va);
            pfn = addPage(type[i]);
            swapIn(spi, pfn);
        }
        else {
            /* 접근한 적 없는 상태 (lpage 의 해당 엔트리가 비어있는 상태) */
            pfn = addPage(type[i]);
            Page* npage = pg_free_list[pfn].page;
            // 새로 만들 수 없는 경우 fail
            if (npage == NULL) return -1;
            // 이전 페이지의 엔트리 업데이트

            printf("\n(fault) pid: %d, va; %d\n", pid, (unsigned char)va);
            printf("(fault) enti[i]: %d\n", enti[i]);
            printf("(fault) lpage->pte[(int)enti[i]]: ");
            pt_entry(lpage->pte[(int)enti[i]]);
            printf("\n");

            lpage->pte[(int)enti[i]] = (pfn << 2) + PRESENT_BIT_MASK;
            
            printf("(fault) lpage->pte[(int)enti[i]]: ");
            pt_entry(lpage->pte[(int)enti[i]]);
            printf("\n");

            if (i == 2) {  // PageTable 에 PageFrame 을 추가할 때 -> pgf_queue 업데이트
                addPGF(pgf_queue, npage, lpage, pfn, enti[i], pcb->pid, (unsigned char)va);
            }
            lpage = npage;
        }
    }
    printf("\n[ AFTER ]:\n");
    pt_pg_free_list();
    pt_sp_list();
    pt_pgf_queue();

    printf("\n:return: 0 (success)\n");

    return 0;
}

void* ku_mmu_init(unsigned int pmem_size, unsigned int swap_size) {
    /*
        pmem_size: 할당할 physical memory 영역의 크기로, 바이트 단위이다
        swap_size: 할당할 스왑 공간의 크기로, 바이트 단위이다
    */
    void* pmem = NULL;
    void* smem = NULL;
    int npage = pmem_size / 4;
    int nswap = swap_size / 4;
    pfl_sz = npage;
    spl_sz = nswap;
    // mem size, swap size 크기 조건 검사
    // 물리 메모리 할당
    pmem = malloc(pmem_size);  // npage 로 하지 않은 것이 에러의 원인이 될 수도 있다
    memset(pmem, 0, pmem_size);
    // 스왑 공간 메모리 할당
    smem = malloc(swap_size);  // nswap 으로 하지 않은 것이 에러의 원인이 될 수도 있다
    memset(smem, 0, swap_size);
    // pg_free_list 초기화 (0 번 페이지는 제외 처리)
    pg_free_list = (PFRI*)malloc(sizeof(PFRI) * npage);
    for (int i = 1; i < npage; ++i) {
        pg_free_list[i].page = (Page *)pmem + i;  // 사용되지 않은 페이지의 엔트리는 전부 0 으로 할당되어 있어야 한다.
        pg_free_list[i].type = P_TYPE_UNDEFINED;
        pg_free_list[i].is_free = TRUE;
    }
    if (npage) {
        pg_free_list[0].page = NULL;
        pg_free_list[0].type = NOT_USED_TYPE;
        pg_free_list[0].is_free = FALSE;
    }
    // sw_free_list 초기화
    sp_list = (SPI*)malloc(sizeof(SPI) * nswap);
    for (int i = 1; i < spl_sz; ++i) {
        sp_list[i].page = (Page *)smem + i;
        sp_list[i].is_free = TRUE;
        sp_list[i].spn = i;
    }
    if (nswap) {
        sp_list[0].page = NULL;
        sp_list[0].pgtable = NULL;
        sp_list[0].spn = 0;
        sp_list[0].is_free = FALSE;
    }
    // pf_queue 초기화 
    pgf_queue = (PGF_Queue*)malloc(sizeof(PGF_Queue));
    pgf_queue->head = NULL;
    pgf_queue->tail = NULL;
    pgf_queue->len = 0;
    // pcb_list 초기화
    pcb_list = (PCB_List*)malloc(sizeof(PCB_List));
    pcb_list->head = NULL;
    pcb_list->tail = NULL;
    pcb_list->len = 0;

    // test print
    printf("\n========================\n");
    printf("<ku_mmu_init test print>\n");
    printf("  pmem: %p\n", pmem);
    printf("  smem: %p\n", smem);
    printf("  npage: %d\n", npage);
    printf("  nswap: %d\n", nswap);
    printf("\n");
    pt_pg_free_list();
    pt_sp_list();
    pt_pgf_queue();

    // 물리 메모리 시작 주소 리턴 (fail 할 경우 0 리턴)
    return pmem;
}

int ku_run_proc1(char pid, void **ku_cr3) {
    /*
        실행하려는 process 의 id 를 받아서, 
        실행했던 적이 있는 process 면 PCB 의 저장된 PageDir 의 '포인터의 시작 주소'를 ku_cr3 에 넣어준다.
        처음 실행하는 process 면 PageDir 을 만들어서 넣어준다.
        그 과정에서 물리 메모리가 PD, PMD, PT 로 가득 차거나, 스왑공간이 가득차면 -1 을 리턴한다.
        정상적으로 종료하면 0 을 리턴한다.

        pid: 실행하고자 하는 process 의 id
        ku_cr3: 해당 process 의 PageDir 포인터의 주소 (Page** 타입)
        
        :return: 성공하면 0, 실패하면 -1

        전제 조건
            - pid 는 실행중인 process 의 id 가 아니고, 교체하고자 하는 process 의 id 이다
            - ku_cr3 = &page (page 의 시작 주소를 담은 포인터의 주소를 넘겨줘야 한다)
        
        (pcb 에 없을 때)
            fail
                - free list 가 꽉 차 있고, PageFrame 이 없을 때
                - free list 가 꽉 차 있고, SwapSpace 가 꽉 찼을 때
            success
                - free list 에 자리가 있을 때
                - free list 에 자리가 없지만, PageFrame 이 있고, SwapSpace 가 있을 때

        알고리즘
            - pid 가 pcb_list 에 있을 때
                - ku_cr3 = &(pcb->pgdir) 하고 리턴 0
            - pid 가 pcb_list 에 존재하지 않을 때
                npcb->pgdir 에 PageDir 할당
                    - pg_free_list 가 남을 경우
                        - npcb->pgdir 에 해당 page 할당, 엔트리 0 으로 초기화
                        - pg_free_list 변경 (getFreePage 에서 알아서 변경)
                            - type = PD_TYPE
                            - is_free = FALSE
                    - 남지 않을 경우
                        - pf_queue 가 있고, swap 공간이 있을 때
                            - swap out
                                - popHeadPGF
                                - page 주소가 아닌, 엔트리 복사
                                - pid, fadd, ladd 복사
                                - is_free = FALSE
                            - npcb->pgdir 에 해당 page 할당, 엔트리 0 으로 초기화
                        - 둘 중 하나라도 없을 때
                            - fail 
    */
    PCB* npcb = searchPCB(pcb_list, pid); 

    // 실행한 적이 없는 pid 일 때
    if (npcb == NULL) {
        // pcb 생성
        npcb = addPCB(pcb_list, pid);
        int pfn = getFreePage(PD_TYPE);
        Page* npage = pg_free_list[pfn].page; 
        // free page 가 있을 때
        if (npage != NULL) {
            setZeroPage(npage);
            npcb->pgdir = npage;
        }
        // free page 가 없을 때 (swap 가능성 확인)
        else {
            PGF* pgf = getPageFrame();
            SPI* spi = getFreeSwapPage();
            if (pgf && spi) {
                // npage = swapOut(pgf, spi);
                // npcb->pgdir = npage;
            }
            else {
                if (pgf) insertHeadPGF(pgf_queue, pgf);
                removeTailPCB(pcb_list);  // addPCB 로 생성되었던 PCB 제거
                return -1;  // fail
            }
        }
    } 

    *ku_cr3 = (void*)(npcb->pgdir);
    return 0;  // success
}

int ku_run_proc2(char pid, void **ku_cr3) {
    PCB* npcb = searchPCB(pcb_list, pid); 

    // 실행한 적이 없는 pid 일 때
    if (npcb == NULL) {
        // pcb 생성
        npcb = addPCB(pcb_list, pid);
        int pfn = getFreePage(PD_TYPE);
        Page* npage = pg_free_list[pfn].page;
        SPI* spi = getFreeSwapPage();
        PGF* pgf = getPageFrame();
        // free page 가 있을 때
        if (npage) {
            setZeroPage(npage);
            npcb->pgdir = npage;
        }
        // PageFrame 이나 SwapSpace 중 하나라도 없으면 fail
        else if (pgf == NULL || spi == NULL) return -1;  // fail
        // PageFrame 과 SwapSpace 둘 다 있을 때
        else {
            popHeadPGF(pgf_queue);
            // npage = swapOut(pgf, spi);
            // npcb->pgdir = npage;
        }
    } 

    *ku_cr3 = (void*)(npcb->pgdir);
    return 0;  // success
}

int ku_run_proc(char pid, void **ku_cr3) {
    PCB* npcb = searchPCB(pcb_list, pid); 

    printf("\n=============================================\n");
    printf("<ku_run_proc(pid: %d, ku_cr3: %p) test print>\n", pid, ku_cr3);
    printf("\npcb_list before:\n");
    pt_pcb_list();

    // 실행한 적이 없는 pid 일 때
    if (npcb == NULL) {
        // pcb 생성
        npcb = addPCB(pcb_list, pid);
        Page* npage = pg_free_list[addPage(PD_TYPE)].page;
        if (npage) npcb->pgdir = npage;
        else {

            printf("\n:return: -1 (fail)\n");

            return -1;
        }
    } 

    printf("\npcb_list after: \n");
    pt_pcb_list();
    pt_pg_free_list();
    pt_sp_list();
    pt_pgf_queue();

    *ku_cr3 = (void*)(npcb->pgdir);

    printf("\n  (proc) ku_cr3: %p, *ku_cr3(pg): %p\n", ku_cr3, *ku_cr3);
    printf("\n:return: 0 (success)\n");

    return 0;  // success
}

int main() {
    int ret = 30;
    ku_mmu_init(64, 128);  // pmem 출력 검사 안 함

    void* ku_cr3;
    ku_run_proc(1, &ku_cr3);

    printf("\n  - - - - - - - - - - - -  \n");
    printf("\n  ku_cr3: %p \n", ku_cr3);
    printf("\n  - - - - - - - - - - - -  \n");

    ku_page_fault(1, 0);
    ku_page_fault(1, 1);
    ku_page_fault(1, 5);
    ku_page_fault(1, 8);
    ku_page_fault(1, 40);
    ku_page_fault(1, 250);

    ku_run_proc(2, &ku_cr3);
    // printf("\n  - - - - - - - - - - - -  \n");
    // printf("\n  ku_cr3: %p \n", ku_cr3);
    // printf("\n  - - - - - - - - - - - -  \n");

    ku_page_fault(2, 0);
    ku_page_fault(2, 3);
    ku_page_fault(2, 15);
    ku_page_fault(2, 43);  

    ku_run_proc(1, &ku_cr3);
    ret = ku_page_fault(1, 0);
    printf("\nret: %d\n", ret);

    ku_run_proc(3, &ku_cr3);
    ret = ku_page_fault(3, 0);
    printf("\nret: %d\n", ret);

    ku_run_proc(4, &ku_cr3);
    ret = ku_page_fault(4, 0);
    printf("\nret: %d\n", ret);

    // Page* pg = (Page*)malloc(sizeof(Page));
    // pg->pte[0] = 34;
    // Page** cr3 = &pg;
    // char temp = *(char*)*cr3;
    // char cva = 0b11001011;
    // unsigned char va = 0b11001011;
    // unsigned char tva = cva;

    // printf("cva: %d\n", cva);
    // printf("va: %d\n", va);
    // printf("tva: %d\n", tva);

    // printf("(cr3: %p) = (&pg: %p)\n", cr3, &pg);
    // printf("(*cr3: %p) = (pg: %p)\n", *cr3, pg);
    // printf("*(char *)*cr3: %d) = (*(char *)pg: %d)\n", *(char *)*cr3, *(char *)pg);
    // printf("yes\n");

    // printf("va: %d\n", va);
    // printf("PD_INDEX: %d\n", (va & PD_MASK) >> PD_SHIFT);
    // printf("PMD_INDEX: %d\n", (va & PMD_MASK) >> PMD_SHIFT);
    // printf("PT_INDEX: %d\n", (va & PT_MASK) >> PT_SHIFT);
    // printf("PO_INDEX: %d\n", va & PO_MASK);

    // printf("PD_MASK: %d\n", PD_MASK);
    // printf("PMD_MASK: %d\n", PMD_MASK);
    // printf("PT_MASK: %d\n", PT_MASK);
    // printf("OFS_MASK: %d\n", PO_MASK);

    // ku_run_proc(1, pmem);

    // ku_mmu_init test print
    // ============================================
    // printf("(main) pmem: %p\n", pmem);
    // printf("(main) kmg_free_list = [ ");
    // for (int i = 0; i < 20; ++i) {
    //     printf("%d ", ku_free_list[i].is_free);
    // }
    // printf("]\n");

    return 0;
}