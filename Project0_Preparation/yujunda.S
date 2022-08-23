.global main
main:
	li s0,0
	li s1,100
	li s2,0
	bge s0,s1,end

loop:
	addi s0,s0,1
	add s2,s2,s0
	blt s0,s1,loop

end:
	j end
