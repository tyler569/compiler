int print(int a);

int a;

int func(int a) {
    print(a); // line 5
    {
        int a = 2;
        print(a); // line 8
    }
    {
        int a = 3;
        print(a); // line 12
        {
            int a = 4;
            print(a); // line 15
        }
        print(a); // line 12
    }
    print(a); // line 5
}

