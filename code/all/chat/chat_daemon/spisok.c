#include <stdio.h>
#include <stdlib.h>

typedef struct NODE{
    int num;
    struct NODE * next;
}Node;

/////////////// main ///////////////////
int main(int argc, char *argv[])
{ 
    Node *end, *begin, *node; // ��������� �� ������, �����, � ����������� �������
    end = begin = (Node*) malloc( sizeof(Node)); // �������� ������ ��� ������� ��������
    begin->next = begin; // �������� �� ����
    int num, // ����� �������� �����
        sum=0; // ����� ����� �����
    while( scanf("%d", &num ) == 1 ){
        // ������ ������...
        end->num = num; // ������ ����� � ��������� �������
        node = (Node*) malloc(sizeof(Node)); // ����� ����
        node->next = begin; // ��������� ������ ���� �� ������
        end->next = node; // ��������� ���������� �� ����� �������
        end = node; // ��������� ����� �� ����� ��.
    }
    puts("\n\n");
    // ���� ����������� ������ � ������� ����� �����
    for( node = begin; node->next != begin; node = node->next ){
        printf("%d\n", node->num );
        sum += node->num;
    }
    printf("\n\nSumma = %d", sum );

    fflush(stdin);
    getchar(); // �����
    return 0;
}