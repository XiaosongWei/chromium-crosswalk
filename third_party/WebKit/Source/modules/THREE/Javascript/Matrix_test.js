
function Matrix4_Test()
{

   var a = new three.Matrix4();
   var b = new three.Matrix4();

   a.set(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
   b.set(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);

   var c = a.mutilplyMatrices(a, b);
   var array = c.elements;

   var result = true;
   if(array[0] != 80)
              return false;
   if(array[1] != 240)
              return false;
   if(array[2] != 400)
              return false;
   if(array[3] != 560)
              return false;

   return true;

}
