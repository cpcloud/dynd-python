import sys
import unittest
from datetime import date
from dynd import nd, ndt

class TestStructAssign(unittest.TestCase):
    def test_single_struct(self):
        a = nd.empty('{x:int32; y:string; z:bool}')
        a[...] = [3, 'test', False]
        self.assertEqual(nd.as_py(a.x), 3)
        self.assertEqual(nd.as_py(a.y), 'test')
        self.assertEqual(nd.as_py(a.z), False)

        a = nd.empty('{x:int32; y:string; z:bool}')
        a[...] = {'x':10, 'y':'testing', 'z':True}
        self.assertEqual(nd.as_py(a.x), 10)
        self.assertEqual(nd.as_py(a.y), 'testing')
        self.assertEqual(nd.as_py(a.z), True)

    def test_nested_struct(self):
        a = nd.empty('{x: 2, int16; y: {a: string; b: float64}; z: 1, cfloat32}')
        a[...] = [[1,2], ['test', 3.5], [3j]]
        self.assertEqual(nd.as_py(a.x), [1, 2])
        self.assertEqual(nd.as_py(a.y.a), 'test')
        self.assertEqual(nd.as_py(a.y.b), 3.5)
        self.assertEqual(nd.as_py(a.z), [3j])

        a = nd.empty('{x: 2, int16; y: {a: string; b: float64}; z: 1, cfloat32}')
        a[...] = {'x':[1,2], 'y':{'a':'test', 'b':3.5}, 'z':[3j]}
        self.assertEqual(nd.as_py(a.x), [1, 2])
        self.assertEqual(nd.as_py(a.y.a), 'test')
        self.assertEqual(nd.as_py(a.y.b), 3.5)
        self.assertEqual(nd.as_py(a.z), [3j])

    def test_single_struct_array(self):
        a = nd.empty('3, {x:int32; y:int32}')
        a[...] = [(0,0), (3,5), (12,10)]
        self.assertEqual(nd.as_py(a.x), [0, 3, 12])
        self.assertEqual(nd.as_py(a.y), [0, 5, 10])

        a[...] = [{'x':1,'y':2}, {'x':4,'y':7}, {'x':14,'y':190}]
        self.assertEqual(nd.as_py(a.x), [1, 4, 14])
        self.assertEqual(nd.as_py(a.y), [2, 7, 190])

        a = nd.empty('2, Var, {count:int32; size:string(1,"A")}')
        a[...] = [[(3, 'X')], [(10, 'L'), (12, 'M')]]
        self.assertEqual(nd.as_py(a.count), [[3], [10, 12]])
        self.assertEqual(nd.as_py(a.size), [['X'], ['L', 'M']])

        a[...] = [[{'count':6, 'size':'M'}],
                        [{'count':3, 'size':'F'}, {'count':16, 'size':'D'}]]
        self.assertEqual(nd.as_py(a.count), [[6], [3, 16]])
        self.assertEqual(nd.as_py(a.size), [['M'], ['F', 'D']])

        a[...] = {'count':1, 'size':'Z'}
        self.assertEqual(nd.as_py(a.count), [[1], [1, 1]])
        self.assertEqual(nd.as_py(a.size), [['Z'], ['Z', 'Z']])

        a[...] = [[(10, 'A')], [(5, 'B')]]
        self.assertEqual(nd.as_py(a.count), [[10], [5, 5]])
        self.assertEqual(nd.as_py(a.size), [['A'], ['B', 'B']])

    def test_nested_struct_array(self):
        a = nd.empty('3, {x:{a:int16; b:int16}; y:int32}')
        a[...] = [((0,1),0), ((2,2),5), ((100,10),10)]
        self.assertEqual(nd.as_py(a.x.a), [0, 2, 100])
        self.assertEqual(nd.as_py(a.x.b), [1, 2, 10])
        self.assertEqual(nd.as_py(a.y), [0, 5, 10])

        a[...] = [{'x':{'a':1,'b':2},'y':5},
                        {'x':{'a':3,'b':6},'y':7},
                        {'x':{'a':1001,'b':110},'y':110}]
        self.assertEqual(nd.as_py(a.x.a), [1, 3, 1001])
        self.assertEqual(nd.as_py(a.x.b), [2, 6, 110])
        self.assertEqual(nd.as_py(a.y), [5, 7, 110])

        a = nd.empty('2, Var, {count:int32; size:{name:string(1,"A"); id: int8}}')
        a[...] = [[(3, ('X', 10))], [(10, ('L', 7)), (12, ('M', 5))]]
        self.assertEqual(nd.as_py(a.count), [[3], [10, 12]])
        self.assertEqual(nd.as_py(a.size.name), [['X'], ['L', 'M']])
        self.assertEqual(nd.as_py(a.size.id), [[10], [7, 5]])

    def test_missing_field(self):
        a = nd.empty('{x:int32; y:int32; z:int32}')
        def assign(x, y):
            x[...] = y
        self.assertRaises(RuntimeError, assign, a, [0, 1])
        self.assertRaises(RuntimeError, assign, a, {'x':0, 'z':1})

    def test_extra_field(self):
        a = nd.empty('{x:int32; y:int32; z:int32}')
        def assign(x, y):
            x[...] = y
        self.assertRaises(RuntimeError, assign, a, [0, 1, 2, 3])
        self.assertRaises(RuntimeError, assign, a, {'x':0,'y':1,'z':2,'w':3})

if __name__ == '__main__':
    unittest.main()
