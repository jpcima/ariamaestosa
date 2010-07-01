#include "unit_test.h"
#include <iostream>
#include <stdio.h>
#include <vector>
#include <map>
#include "wx/string.h"
#include "wx/tokenzr.h"
#include "wx/filename.h"

namespace TestCaseList
{
    struct Node
    {
        std::string m_name;
        std::vector<UnitTestCase*> m_test_cases;
        std::map<wxString, Node> m_children;
        
        Node()
        {
        }
        Node(const char* name)
        {
            m_name = name;
        }
    };
    
    Node* root = NULL;
    
    void add(UnitTestCase* testCase, const std::vector<wxString>& path)
    {
        if (root == NULL)
        {
            root = new Node("All");
        }
        
        Node* currNode = root;
        for (unsigned int n=0; n<path.size(); n++)
        {
            if (currNode->m_children.find(path[n]) == currNode->m_children.end())
            {
                currNode->m_children[path[n]] = Node(path[n]);
            }
            currNode = &currNode->m_children[path[n]];
        }
        currNode->m_test_cases.push_back(testCase);
    }
    
    std::map<int, Node*> testGroupsById;
    std::map<int, UnitTestCase*> testCasesById;

}

UnitTestCase::UnitTestCase(const char* name, const char* filePath)
{
    //if (TestCaseList::all_test_cases == NULL) TestCaseList::all_test_cases = new std::vector<UnitTestCase*>();
    
    m_name = name;
    
    wxString filePathWxString(filePath);
    if (filePathWxString.Find('.') != wxNOT_FOUND)
    {
        filePathWxString = filePathWxString.BeforeLast('.');
    }
    
    std::vector<wxString> path;
    wxStringTokenizer tokenizer(filePathWxString, wxFileName::GetPathSeparator());
    while (tokenizer.HasMoreTokens())
    {
        wxString token = tokenizer.GetNextToken();
        path.push_back(token);
    }
    
    
    TestCaseList::add(this, path);
}

UnitTestCase::~UnitTestCase()
{
    // TODO: cleanup!
    /*
    for (unsigned int n=0; n<TestCaseList::all_test_cases->size(); n++)
    {
        if ((*TestCaseList::all_test_cases)[n] == this)
        {
            TestCaseList::all_test_cases->erase( TestCaseList::all_test_cases->begin() + n );
            break;
        }
    }
    
    if (TestCaseList::all_test_cases->size() == 0) delete TestCaseList::all_test_cases;
     */
}

void runTest(UnitTestCase* testCase)
{
    std::cout << "Running test case " << testCase->getName() << "... ";
    std::cout.flush();
    
    bool passed = true;
    
    try
    {
        testCase->run();
    }
    catch (const std::exception& ex)
    {
        passed = false;
        std::cout << "FAILED : " << ex.what() << std::endl;
    }
    
    if (passed) std::cout << "passed" << std::endl;    
}

static int id = 1;

void printTree(TestCaseList::Node* currNode, int ident)
{
    //for (int n=0; n<ident; n++) std::cout << "    ";
    //std::cout << "(0) " << currNode->m_name << std::endl;
    
    
    for (std::map<wxString, TestCaseList::Node>::iterator it = currNode->m_children.begin();
         it != currNode->m_children.end();
         it++)
    {
        for (int i=0; i<=ident; i++) std::cout << "    ";
        std::cout << "(" << id << ") [group] " << (*it).second.m_name << std::endl;
        TestCaseList::testGroupsById[id] = &(*it).second;
        id++;
        
        printTree(&((*it).second), ident+1);
    }
    
    for (unsigned int n=0; n<currNode->m_test_cases.size(); n++)
    {
        for (int i=0; i<=ident; i++) std::cout << "    ";
        std::cout << "(" << id << ") [test] " << currNode->m_test_cases[n]->getName() << std::endl;
        TestCaseList::testCasesById[id] = currNode->m_test_cases[n];
        id++;
    }
}

void runTestsIn(TestCaseList::Node* node)
{
    for (unsigned int n=0; n<node->m_test_cases.size(); n++)
    {
        runTest(node->m_test_cases[n]);
    }
    
    for (std::map<wxString, TestCaseList::Node>::iterator it = node->m_children.begin();
         it != node->m_children.end();
         it++)
    {
        runTestsIn(&(*it).second);
    }
}

void UnitTestCase::showMenu()
{
    TestCaseList::testCasesById.clear();
    TestCaseList::testGroupsById.clear();
    id = 1;
    
    TestCaseList::Node* from = TestCaseList::root;
    while (from->m_children.size() == 1)
    {
        from = &(from->m_children.begin()->second);
    }
    
    std::cout << "==== UNIT TESTS ===\n";
    std::cout << "(0) [group] All Tests\n";
    printTree(from, 0);
    
    std::cout << "----\n";
    std::cout << "Make a choice : \n";
    std::cout << "\n> ";
    fflush(stdout);
    
    int choice;
    std::cin >> choice;
    
    if (choice == 0)
    {
        runTestsIn(TestCaseList::root);
    }
    else if (TestCaseList::testGroupsById.find(choice) != TestCaseList::testGroupsById.end())
    {
        runTestsIn(TestCaseList::testGroupsById[choice]);
    }
    else if (TestCaseList::testCasesById.find(choice) != TestCaseList::testCasesById.end())
    {
        runTest(TestCaseList::testCasesById[choice]);
    }
    else
    {
        std::cerr << "Invalid input\n";
    }
}

